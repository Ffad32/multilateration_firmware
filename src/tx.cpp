#include <Arduino.h>
#include "tx.h"
#include "battery.h"        /* battery_read_voltage() — included for the report */
#include "chip_config.h"    /* chip_config_lookup() — to get the chip ID string  */
#include "wifi_reporter.h"  /* wifi_report() — sends data to the REST server     */

/* Frames used in the ranging process. See NOTE 3 in tx.h. */
static uint8_t tx_msg[]    = {0x41, 0x88, 0, 0xCA, 0xDE, 'T', '0', 'R', '0', 0xE0, 0, 0};
static uint8_t rx_msg[]    = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Buffer to store received response message. */
static uint8_t tx_rx_buffer[TX_RX_BUF_LEN];

/* Hold copies of computed time of flight and distance here for reference so
 * that they can be examined at a debug breakpoint. */
static double tof;
static double distance;

/* Per-anchor distance results (anchors 1..NUM_ANCHORS). */
static double anchor_distance[NUM_ANCHORS + 1]; /* index 0 unused; 1-based */

/* Current anchor index being ranged to (1..NUM_ANCHORS). */
static int anchor_idx = 1;

void setup_tx() {
    UART_init();
    test_run_info((unsigned char *)APP_NAME);

    /* Configure SPI and reset DW IC. */
    spiBegin(PIN_IRQ, PIN_RST);
    spiSelect(PIN_SS);

    /* Wait for DW3000 to start up (INIT_RC → IDLE_RC transition). */
    delay(500);
     dwt_softreset();
    //delay(300); /* Wait for chip to stabilize after soft reset. */

    delay(300);
        bool idle_check_passed = false;
    for (int retries = 0; retries < 10; retries++) {
        if (dwt_checkidlerc()) {
            idle_check_passed = true;
            break;
        }
        delay(100);
    }

    /* Verify DW IC is in IDLE_RC before proceeding. */
    if (!dwt_checkidlerc()) {
        UART_puts("IDLE FAILED\r\n");
        while (1) ;
    }

   
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        UART_puts("INIT FAILED\r\n");
        while (1) ;
    }

    /* Enable LEDs for debug: D1 flashes on each TX on DW3000 red eval-shield boards. */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 13 in tx.h. */
    if (dwt_configure(&config)) {
        /* dwt_configure() returns DWT_ERROR if PLL or RX calibration failed;
         * the host should reset the device. */
        UART_puts("CONFIG FAILED\r\n");
        while (1) ;
    }

    /* Configure the TX spectrum parameters (power, PG delay and PG count). */
    dwt_configuretxrf(&txconfig_options);

    /* Apply default antenna delay values. See NOTE 2 in tx.h. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Set expected response delay and timeout. See NOTE 1 and 5 in tx.h.
     * These values are fixed for all exchanges so they are set once here. */
    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

    /* Enable TX/RX states output on GPIOs 5 and 6 for debug.
     * Note: in real low-power applications the LNA/PA should not be left enabled. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);
}

void loop_tx() {
    /* Embed the current anchor index into the poll message (byte 8). */
    tx_msg[8] = (uint8_t)anchor_idx;
    tx_msg[ALL_MSG_SN_IDX] = frame_seq_nb;

    /* Write frame data to DW IC and prepare transmission. See NOTE 7 in tx.h. */
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
    dwt_writetxdata(sizeof(tx_msg), tx_msg, 0); /* Zero offset in TX buffer. */
    dwt_writetxfctrl(sizeof(tx_msg), 0, 1);     /* Zero offset in TX buffer, ranging. */

    /* Start transmission; reception is enabled automatically after the frame
     * is sent and the delay set by dwt_setrxaftertxdelay() has elapsed. */
    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    /* Poll for reception of a frame or error/timeout. See NOTE 8 in tx.h. */
    while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
             (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
    }

    /* Increment frame sequence number after transmission (modulo 256). */
    frame_seq_nb++;

    if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
        /* Clear good RX frame event in the DW IC status register. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

        /* A frame has been received — read it into the local buffer. */
        uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
        if (frame_len <= sizeof(tx_rx_buffer)) {
            dwt_readrxdata(tx_rx_buffer, frame_len, 0);

            /* Check that the frame is the expected response from the SS TWR responder.
             * The sequence number field is cleared before comparison to simplify
             * validation. */
            tx_rx_buffer[ALL_MSG_SN_IDX] = 0;
            if (memcmp(tx_rx_buffer, rx_msg, ALL_MSG_COMMON_LEN) == 0) {
                uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
                int32_t  rtd_init, rtd_resp;

                /* Retrieve poll transmission and response reception timestamps.
                 * See NOTE 9 in tx.h. */
                poll_tx_ts = dwt_readtxtimestamplo32();
                resp_rx_ts = dwt_readrxtimestamplo32();

                /* Read carrier integrator value and calculate clock offset ratio.
                 * See NOTE 11 in tx.h. */
                float clockOffsetRatio =
                    ((float)dwt_readclockoffset()) / (float)(1UL << 26);

                /* Get timestamps embedded in response message. */
                resp_msg_get_ts(&tx_rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
                resp_msg_get_ts(&tx_rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

                /* Compute time of flight and distance, using clock offset ratio to
                 * correct for differing local and remote clock rates. */
                rtd_init = resp_rx_ts - poll_tx_ts;
                rtd_resp = resp_tx_ts - poll_rx_ts;

                tof      = ((rtd_init - rtd_resp * (1.0f - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
                distance = tof * SPEED_OF_LIGHT;

                /* Store result for this anchor. */
                if (anchor_idx >= 1 && anchor_idx <= NUM_ANCHORS) {
                    anchor_distance[anchor_idx] = distance;
                }

                /* Display computed distance. */
                char dist_str[32];
                snprintf(dist_str, sizeof(dist_str), "A%d DIST: %3.2f m\n", anchor_idx, distance);
                test_run_info((unsigned char *)dist_str);
            }
        }
    } else {
        /* Clear RX error/timeout events in the DW IC status register. */
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
    }

    /* Advance to the next anchor; after the last anchor print all results,
     * send a WiFi report, then wait before starting the next ranging cycle. */
    anchor_idx++;
    if (anchor_idx > NUM_ANCHORS) {
        anchor_idx = 1;

        /* Print all anchor distances to Serial for local monitoring. */
        Serial.printf("%3.2f m  %3.2f m  %3.2f m\n",
                      anchor_distance[1],
                      anchor_distance[2],
                      anchor_distance[3]);

        /* Build the chip ID string (12 hex digits = 48-bit MAC). */
        uint64_t mac = 0;
        {
            /* Read the 6-byte efuse MAC and pack it into a uint64_t. */
            uint8_t m[6] = {0};
            esp_efuse_mac_get_default(m);
            for (int k = 0; k < 6; k++) {
                mac = (mac << 8) | m[k];
            }
        }
        char chip_id_str[13]; /* 12 hex digits + null terminator */
        snprintf(chip_id_str, sizeof(chip_id_str), "%04X%08X",
                 (uint32_t)(mac >> 32),
                 (uint32_t)(mac & 0xFFFFFFFFULL));

        /* Read the current battery voltage. */
        float bat_v = battery_read_voltage();

        /* Send the report to the REST server. */
        wifi_report(chip_id_str, anchor_distance, NUM_ANCHORS, bat_v);

        /* Wait before starting the next ranging cycle. */
        delay(RNG_DELAY_MS);
    }
}

// ---------------------------------------------------------------------------
// tx_get_anchor_distances
// ---------------------------------------------------------------------------
/**
 * Return a read-only pointer to the internal anchor_distance array.
 * Index 0 is unused; valid entries are [1]..[NUM_ANCHORS].
 */
const double *tx_get_anchor_distances() {
    return anchor_distance;
}

// Anchor positions (for reference):
// anch1: (3000, 0, 0)
// anch2: (3000, ?, ?)
// anch3: (3000, ?, ?)
