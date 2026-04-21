#include <Arduino.h>
#include "battery.h"

float battery_read_voltage() {
    uint32_t raw_sum = 0;
    for (int i = 0; i < 8; i++) {
        raw_sum += analogRead(BAT_PIN);
    }
    float raw = raw_sum / 8.0f;
    float v_adc = (raw / 4095.0f) * 3.3f;
    float v_bat = v_adc * 2.0f;
    return v_bat;
}

void battery_print_status() {
    float voltage = battery_read_voltage();
    Serial.printf("Battery: %.2f V", voltage);
    if (voltage < BAT_CRITICAL_VOLTAGE) {
        Serial.print(" [CRITICAL]");
    } else if (voltage < BAT_LOW_VOLTAGE) {
        Serial.print(" [LOW]");
    } else {
        Serial.print(" [OK]");
    }
    Serial.println();
}
