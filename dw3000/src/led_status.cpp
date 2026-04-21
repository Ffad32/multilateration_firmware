#include "led_status.h"
#include <Arduino.h>
#include "battery.h"

#define LED_STATUS_PIN 25

// State variables
static bool wifi_alert_active = false;
static uint32_t wifi_alert_start = 0;   // millis when wifi_failed was called
static bool low_battery = false;
static bool led_state = false;
static uint32_t last_blink_toggle = 0;  // millis of last blink toggle

void led_status_init() {
    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, LOW);
    led_state = false;
    wifi_alert_active = false;
    low_battery = false;
    last_blink_toggle = 0;
}

void led_status_wifi_failed() {
    wifi_alert_active = true;
    wifi_alert_start = millis();
}

void led_status_update(float battery_voltage) {
    low_battery = (battery_voltage < BAT_LOW_VOLTAGE);
}

void led_status_tick() {
    uint32_t now = millis();

    // Priority: WiFi alert (solid ON) overrides blink
    if (wifi_alert_active) {
        if (now - wifi_alert_start < 30000UL) {
            // Within 30 seconds of WiFi failure -> LED ON
            if (!led_state) {
                digitalWrite(LED_STATUS_PIN, HIGH);
                led_state = true;
            }
            return; // WiFi alert active, ignore battery blink
        } else {
            // 30 seconds elapsed, clear WiFi alert
            wifi_alert_active = false;
            // LED will be turned off below if no other condition
        }
    }

    // Low battery blink (only if WiFi alert not active)
    if (low_battery) {
        if (now - last_blink_toggle >= 500UL) {
            last_blink_toggle = now;
            led_state = !led_state;
            digitalWrite(LED_STATUS_PIN, led_state ? HIGH : LOW);
        }
        return;
    }

    // Neither condition active -> ensure LED is OFF
    if (led_state) {
        digitalWrite(LED_STATUS_PIN, LOW);
        led_state = false;
    }
}