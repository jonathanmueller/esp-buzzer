#pragma once
#include "_config.h"

#define FASTLED_INTERNAL
#include <FastLED.h>

enum color_t : uint8_t {
    COLOR_RED,
    COLOR_YELLOW,
    COLOR_ORANGE,
    COLOR_GREEN,
    COLOR_TEAL,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_WHITE,
    COLOR_NUM,
    COLOR_RGB = 255
};


extern color_t buzzer_color;
extern CRGB buzzer_color_rgb;
extern CRGB colors[];

extern CRGB leds[NUM_LEDS];

void led_setup();