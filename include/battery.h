#pragma once

#include <stdint.h>

extern uint32_t battery_voltage; // [mV]
extern float battery_percent; // [0-1]

void battery_setup();
void battery_loop();