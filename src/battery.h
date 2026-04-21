#pragma once

#define BAT_PIN             34
#define BAT_LOW_VOLTAGE     3.5f
#define BAT_CRITICAL_VOLTAGE 3.3f

float battery_read_voltage();
//void  battery_print_status();
