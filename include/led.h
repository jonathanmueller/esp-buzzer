#pragma once

#define FASTLED_INTERNAL
#include <FastLED.h>

#define LED_PIN D4
#define NUM_LEDS 14

#define FLASH_EFFECT_COLOR baseColor // CRGB::White
#define FLASH_EFFECT_PRE_FLASH_COUNT 4
#define FLASH_EFFECT_PRE_FLASH_DURATION 100
#define FLASH_EFFECT_DURATION 600

enum color_t : uint8_t {
    COLOR_RED,
    COLOR_YELLOW,
    COLOR_ORANGE,
    COLOR_GREEN,
    COLOR_TEAL,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_WHITE,
    COLOR_NUM
};

extern color_t buzzer_color;

extern CRGB leds[NUM_LEDS];

void led_setup();