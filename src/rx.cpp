#include <Arduino.h>
#include "rx.h"
#include "wifi_reporter.h"

/* Frames used in the ranging process. See NOTE 3 in rx.h. */
static uint8_t rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'T', '0', 'R', 0,    0xE0, 0, 0};
static uint8_t tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A',  0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Buffer to store received messages. */
static uint8_t rx_buffer[RX_BUF_LEN];

/* Timestamps of frames transmission/reception. */
static uint64_t poll_rx_ts;
static uint64_t resp_tx_ts;

/* Battery reporting interval (30 seconds). */
#define ANCHOR_BATTERY_REPORT_INTERVAL_MS 30000UL
static uint32_t last_battery_report = 0;

void setup_rx(u_int8_t anchor_number) {
    rx_poll_msg[8] = anchor_number;
    UART_init();
    test_run_info((unsigned char *)APP_NAME);

    /* Configure SPI and reset DW IC. */
    spiBegin(PIN_IRQ, PIN_RST);
    spiSelect(PIN_SS);

    /* Initial delay for chip to power up. */
    delay(500);

    /* Perform soft reset to ensure clean state. */
    dwt_softreset();
    delay(300); /* Wait for chip to stabilize after soft reset. */

    /* Check if chip is in IDLE_RC state with retry logic. */
    bool idle_check_passed = false;
    for (int retries = 0; retries < 10; retries++) {
        if (dwt_checkidlerc()) {
            idle_check_passed = true;
            break;
        }
        delay(100);
    }

    if (!idle_check_passed) {
        UART_puts("IDLE FAILED\r\n");
        while (1) ;
    }

    dwt_softreset();
    delay(200);

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        UART_puts("INIT FAILED\r\n");
        while (1) ;
    }

    /* Enable LEDs for debug: D1 flashes on each TX on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 6 in rx.h. */
    if (dwt_configure(&config)) {
        /* dwt_configure() returns DWT_ERROR if PLL or RX calibration failed;
         * the host should reset the device. */
        UART_puts("CONFIG FAILED\r\n");
        while (1) ;
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count). */
    dwt_configuretxrf(&txconfig_options);
    //dwt_configureframefilter(DWT_FF_DISABLE, 0);

    /* Apply default antenna delay values. See NOTE 2 in rx.h. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Enable TX/RX states output on GPIOs 5 and 6 for debug.
     * Note: in real low-power applications the LNA/PA should not be left enabled. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
}

void loop_rx() {
    /* Periodic battery WiFi reporting (non‑blocking). */
    uint32_t now = millis();
    if (now - last_battery_report >= ANCHOR_BATTERY_REPORT_INTERVAL_MS) {
        last_battery_report = now;
        wifi_report_anchor_battery();
    }

    /* Activate reception immediately. */
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    /* Poll for reception of a frame or error/timeout. See NOTE 6 in rx.h. */
    while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
             (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR))) {
    }

    if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
        /* Clear good RX frame event in the DW IC status register. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

        /* A frame has been received — read it into the local buffer. */
        uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
        if (frame_len <= sizeof(rx_buffer)) {
            dwt_readrxdata(rx_buffer, frame_len, 0);

            /* Check that the frame is a poll sent by the SS TWR initiator.
             * The sequence number field is cleared before comparison to
             * simplify validation. */
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(rx_buffer, rx_poll_msg, ALL_MSG_COMMON_LEN) == 0) {
                /* Retrieve poll reception timestamp. */
                poll_rx_ts = get_rx_timestamp_u64();

                /* Compute response message transmission time. See NOTE 7 in rx.h. */
                uint32_t resp_tx_time =
                    (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
                dwt_setdelayedtrxtime(resp_tx_time);

                /* Response TX timestamp = programmed time + antenna delay. */
                resp_tx_ts = (((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;

                /* Write all timestamps into the response message. See NOTE 8 in rx.h. */
                resp_msg_set_ts(&tx_resp_msg[RESP_MSG_POLL_RX_TS_IDX], poll_rx_ts);
                resp_msg_set_ts(&tx_resp_msg[RESP_MSG_RESP_TX_TS_IDX], resp_tx_ts);

                /* Write and send the response message. See NOTE 9 in rx.h. */
                tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
                dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0); /* Zero offset in TX buffer. */
                dwt_writetxfctrl(sizeof(tx_resp_msg), 0, 1);          /* Zero offset, ranging. */
                int ret = dwt_starttx(DWT_START_TX_DELAYED);

                /* If dwt_starttx() returns an error, abandon this ranging exchange
                 * and proceed to the next one. See NOTE 10 in rx.h. */
                if (ret == DWT_SUCCESS) {
                    /* Poll DW IC until TX frame sent event set. See NOTE 6 in rx.h. */
                    while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS_BIT_MASK)) {
                    }

                    /* Clear TXFRS event. */
                    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);

                    /* Increment frame sequence number after transmission (modulo 256). */
                    frame_seq_nb++;
                }
            }
        }
    } else {
        /* Clear RX error events in the DW IC status register. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_ERR);
    }
}
