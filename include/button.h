#pragma once

#include "_config.h"

enum node_state_t : uint8_t {
    STATE_IDLE,
    STATE_DISABLED,
    STATE_BUZZER_ACTIVE,
    STATE_SHUTDOWN,
    STATE_SHOW_BATTERY,
    STATE_CONFIG
};

extern node_state_t current_state;
extern unsigned long buzzer_active_until;
extern unsigned long buzzer_disabled_until;

void button_setup();
void button_loop();