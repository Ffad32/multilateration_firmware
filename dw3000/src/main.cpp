#include <Arduino.h>
#include <WiFi.h>           /* WiFi.status()                                    */
#include "battery.h"        /* battery_print_status(), battery_read_voltage()  */
#include "chip_config.h"    /* chip_config_lookup(), chip_config_print()        */
#include "wifi_reporter.h"  /* wifi_reporter_init()                             */
#include "led_status.h"     /* LED status signalization                         */
#include "rx.h"             /* setup_rx(), loop_rx()                            */
#include "tx.h"             /* setup_tx(), loop_tx()                            */

/* Active configuration resolved at boot from the chip ID table.
 * Stored globally so loop() can dispatch to the correct role handler. */
static chip_config_t g_cfg;

void setup() {
    /* Initialise Serial at 115200 baud for debug output. */
    Serial.begin(115200);
    delay(200); /* Allow Serial to settle before the first print. */

    /* Initialise LED status signalization (GPIO 25) */
    led_status_init();

    /* Print battery voltage immediately so we know the power state at boot. */
    battery_print_status();

    /* Connect to WiFi.  Blocks up to WIFI_CONNECT_TIMEOUT_MS ms.
     * If the connection fails the device continues without WiFi. */
    wifi_reporter_init();

    /* If WiFi failed to connect, trigger LED alert */
    if (WiFi.status() != WL_CONNECTED) {
        led_status_wifi_failed();
    }

    /* Resolve this board's role (TX/RX) and device number from its chip ID.
     * The chip ID is the 48-bit efuse MAC address. */
    g_cfg = chip_config_lookup();
    chip_config_print(g_cfg);

    if (!g_cfg.found) {
        /* Unknown chip — print the ID so the user can add it to
         * src/chip_config.cpp and reflash, then halt. */
        Serial.println("ERROR: chip ID not in table. "
                       "Add it to src/chip_config.cpp and reflash.");
        while (1) ;
    }

    if (g_cfg.role == ROLE_TX) {
        /* Initiator / tag: polls anchors and computes distances. */
        Serial.printf("Starting as TX (tag #%u)\r\n", g_cfg.number);
        setup_tx();
    } else {
        /* Responder / anchor: replies to poll messages. */
        Serial.printf("Starting as RX (anchor #%u)\r\n", g_cfg.number);
        setup_rx(g_cfg.number);
    }
}

void loop() {
    /* Update LED status (non-blocking blink / WiFi alert) */
    led_status_tick();

    /* Print battery status every 30 seconds (non-blocking). */
    static uint32_t last_bat_check = 0;
    uint32_t now = millis();
    if (now - last_bat_check >= 30000UL) {
        last_bat_check = now;
        battery_print_status();
        led_status_update(battery_read_voltage());
    }

    /* Dispatch to the role-specific loop handler. */
    if (g_cfg.role == ROLE_TX) {
        loop_tx(); /* Ranging cycle; WiFi report is sent inside loop_tx(). */
    } else {
        loop_rx(); /* Wait for poll, send response. */
    }
}
