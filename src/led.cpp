#include "led.h"
#include "button.h"
#include "battery.h"

CRGB leds[NUM_LEDS];


void led_task(void* param);

void led_setup() {
    pinMode(LED_PIN, OUTPUT);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    fill_solid(leds, NUM_LEDS, 0);

    xTaskCreate(&led_task, "led_loop", 2000, NULL, 1, NULL);
}

static uint16_t hue = 0;
static uint8_t angle = 0;
void led_task(void* param) {
    CRGB color;
    while (true) {
        angle += 1;
        hue += 1;


        // if (current_state == STATE_SHUTDOWN) {
        //     fill_solid(leds, NUM_LEDS, 0);
        // } else 
        if (low_battery) {
            fill_solid(leds, NUM_LEDS, (millis() / 100) % 2 == 0 ? CRGB(20,0,0) : 0);
        } else {
            color = CRGB::OrangeRed;
            switch (current_state) {
                case STATE_IDLE:
                    fill_solid(leds, NUM_LEDS, color.nscale8_video(20));
                    break;
                case STATE_SHUTDOWN:
                    fill_solid(leds, NUM_LEDS, color.nscale8_video(1));
                    break;
                case STATE_DISABLED:
                    fill_solid(leds, NUM_LEDS, 0);
                    break;
                case STATE_BUZZER_ACTIVE:
                    for (uint8_t i = 0; i < NUM_LEDS; i++) {
                        leds[i] = color.scale8(sin8(angle + 2*i * 255.0f / NUM_LEDS));
                    }
                    break;
                default:
                    fill_solid(leds, NUM_LEDS, CRGB(0,0,255));

                    for (uint8_t i = 0; i < NUM_LEDS * battery_percent; i++) {
                        uint8_t gr = i * 255.0f / NUM_LEDS;
                        leds[i] = CRGB(gr, 255-gr, 0);
                    }

                    break;
            }
        }

        FastLED.show();
        vTaskDelay(10 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}