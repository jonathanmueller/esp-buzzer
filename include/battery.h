#pragma once

#include <stdint.h>

extern uint32_t battery_voltage; // [mV]
extern float battery_percent; // [0-1]
extern uint8_t battery_percent_rounded; // [0-100]
extern bool low_battery;

void battery_setup();
void battery_loop();

void shutdown(bool turnOffLEDs, bool allowWakeupWithBuzzer);