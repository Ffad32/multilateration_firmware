#pragma once

// LED on GPIO 25
// - WiFi not connected: solid ON for 30 seconds (one-shot after failed init)
// - Low battery: blink 500ms on / 500ms off continuously while battery is low

void led_status_init();

// Call once after wifi_reporter_init() if WiFi failed to connect
void led_status_wifi_failed();

// Call every 30 seconds from main loop with current battery voltage
// Uses BAT_LOW_VOLTAGE threshold from battery.h
void led_status_update(float battery_voltage);

// Call from main loop as fast as possible (handles non-blocking blink timing)
void led_status_tick();