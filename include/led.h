#pragma once

#include <FastLED.h>

#define LED_PIN D4
#define NUM_LEDS 14

extern CRGB leds[NUM_LEDS];

void led_setup();