#pragma once

#include <Arduino.h>
#include "_config.h"

enum led_effect_t : uint8_t {
    EFFECT_NONE,            // Only increase brightness
    EFFECT_FLASH_WHITE,     // Flash with a bright white light
    EFFECT_FLASH_BASE_COLOR // Flash with the buzzer's base color
};

typedef struct {
    uint16_t buzzer_active_time;              // [ms] Duration the buzzer is kept active (0: singular event, 65535: never reset)
    uint16_t deactivation_time_after_buzzing; // [ms] Duration the buzzer is deactivated after buzzer_active_time has passed (0: can press again immediately, 65535: keep disabled forever)
    led_effect_t buzz_effect;                 // The effect to play when pressing the buzzer
    bool can_buzz_while_other_is_active;      // Whether or not we can buzz while another buzzer is active
    bool must_release_before_pressing;        // Whether or not we have to release the buzzer before pressing to register

    uint16_t crc;                             // CRC-16/GENIBUS of the game config (using esp_rom_crc16_be over all previous bytes)
} __attribute__((packed)) game_config_t;

void button_setup();
void button_loop();