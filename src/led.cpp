#include "led.h"
#include "button.h"
#include "battery.h"
#include "nvm.h"

CRGB leds[NUM_LEDS];
color_t buzzer_color = COLOR_ORANGE;

CRGB colors[] = {
    CRGB::Red,
    CRGB::Gold,
    CRGB::OrangeRed,
    CRGB::Olive,
    CRGB::Teal,
    CRGB::Navy,
    CRGB::Purple,
    CRGB::White
};

void led_task(void* param);

void led_setup() {
    pinMode(LED_PIN, OUTPUT);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    fill_solid(leds, NUM_LEDS, 0);

    buzzer_color = nvm_data.color;
    if (buzzer_color >= COLOR_NUM) {
        buzzer_color = COLOR_ORANGE;
    }

    xTaskCreate(&led_task, "led_loop", 2000, NULL, 1, NULL);
}

static uint16_t hue = 0;
static uint8_t angle = 0;
static unsigned long buzzer_start_time = -1UL;
static uint8_t number_of_flashes = 0;

void led_task(void* param) {
    CRGB baseColor;
    unsigned long time;
    while (true) {
        time = millis();
        angle += 1;
        hue += 1;

        baseColor = colors[buzzer_color];

        if (current_state != STATE_BUZZER_ACTIVE) {
            buzzer_start_time = -1UL;
        }

        // if (current_state == STATE_SHUTDOWN) {
        //     fill_solid(leds, NUM_LEDS, 0);
        // } else 
        if (low_battery) {
            fill_solid(leds, NUM_LEDS, (millis() / 100) % 2 == 0 ? CRGB(20,0,0) : 0);
        } else {

            switch (current_state) {
                case STATE_IDLE:
                    fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(20));
                    break;
                case STATE_CONFIG:
                    fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(sin8(millis() / 2) / 4 + 20));
                    break;
                case STATE_SHUTDOWN:
                    fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(1));
                    break;
                case STATE_DISABLED:
                    fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(1));
                    break;
                case STATE_BUZZER_ACTIVE:
                    for (uint8_t i = 0; i < NUM_LEDS; i++) {
                        leds[i] = baseColor;
                        leds[i].nscale8_video(sin8(angle + 2*i * 255.0f / NUM_LEDS));
                    }

                    if (buzzer_start_time == -1UL) {
                        buzzer_start_time = time;
                        number_of_flashes = 0;
                    }

                    if (time - buzzer_start_time < FLASH_EFFECT_DURATION + (FLASH_EFFECT_PRE_FLASH_COUNT*FLASH_EFFECT_PRE_FLASH_DURATION)) {
                        fract8 fract;
                        if (time - buzzer_start_time < (FLASH_EFFECT_PRE_FLASH_COUNT*FLASH_EFFECT_PRE_FLASH_DURATION)) {
                            fract = 255 - (uint8_t)(255.0f * (time - buzzer_start_time) / FLASH_EFFECT_PRE_FLASH_DURATION);
                        } else {
                            fract = 255 - (255.0f * (time - buzzer_start_time - (FLASH_EFFECT_PRE_FLASH_COUNT*FLASH_EFFECT_PRE_FLASH_DURATION)) / FLASH_EFFECT_DURATION);
                        }
                        for (uint8_t i = 0; i < NUM_LEDS; i++) {
                            leds[i] = leds[i].lerp8(FLASH_EFFECT_COLOR, fract);
                        }
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