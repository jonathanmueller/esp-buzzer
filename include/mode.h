#pragma once
#include "Arduino.h"

enum node_state_t : uint8_t {
    STATE_DEFAULT,
    STATE_SHUTDOWN,
    STATE_SHOW_BATTERY,
    STATE_CONFIG
};

enum node_mode_t : uint8_t {
    MODE_DEFAULT,
    MODE_SIMON_SAYS,
    NUM_MODES
};

/* Make sure the default mode is always 0 (the first one) */
typedef enum : uint8_t {
    MODE_DEFAULT_STATE_IDLE,
    MODE_DEFAULT_STATE_DISABLED,
    MODE_DEFAULT_STATE_BUZZER_ACTIVE,
} node_state_default_t;

typedef enum : uint8_t {
    MODE_SIMON_SAYS_IDLE
} node_state_simon_says_t;

typedef union {
    uint8_t raw;
    node_state_default_t node_state_default;
    node_state_simon_says_t node_state_simon_says;
} __attribute__((packed)) node_mode_state_t;

extern node_state_t current_state;
extern unsigned long last_state_change;
void mode_setup();
void set_state(node_state_t state);