#pragma once

#define BUZZER_ACTIVE_TIME 5000
#define BUZZER_DISABLED_TIME 3000


#define BOOT_BUTTON_PIN D9
#define BUZZER_BUTTON_PIN D0
#define BACK_BUTTON_PIN D1

enum node_state_t : uint8_t {
    STATE_IDLE,
    STATE_DISABLED,
    STATE_BUZZER_ACTIVE,
    STATE_SHUTDOWN,
    STATE_CONFIG
};

extern node_state_t current_state;
extern unsigned long buzzer_active_until;
extern unsigned long buzzer_disabled_until;

void button_setup();
void button_loop();