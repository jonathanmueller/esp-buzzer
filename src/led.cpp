#include "led.h"
#include "button.h"
#include "battery.h"
#include "nvm.h"
#include "modes/IMode.h"

CRGB leds[NUM_LEDS];
color_t buzzer_color  = COLOR_ORANGE;
CRGB buzzer_color_rgb = CRGB::OrangeRed;

CRGB colors[] = {
    CRGB::Red,
    CRGB::Yellow,
    CRGB::OrangeRed,
    CRGB::Green,
    CRGB::Teal,
    CRGB::Navy,
    CRGB::Purple,
    CRGB::White
};

void led_task(void *param);

void led_setup() {
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)LED_ENABLE_PIN);

    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_ENABLE_PIN, OUTPUT);

    // Connect MOSFET to power
    digitalWrite(LED_ENABLE_PIN, LOW);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(3.7, MAX_CURRENT);
    FastLED.setBrightness(255);

    buzzer_color     = nvm_data.color;
    buzzer_color_rgb = CRGB(nvm_data.rgb[0], nvm_data.rgb[1], nvm_data.rgb[2]);
    if (buzzer_color >= COLOR_NUM && buzzer_color != COLOR_RGB) {
        buzzer_color = COLOR_ORANGE;
    }

    fill_solid(leds, NUM_LEDS, 0);

    xTaskCreate(&led_task, "led_loop", 2000, NULL, TASK_PRIO_LED, NULL);
}

void fadeTo(CRGB &rgb, const CRGB &other, uint8_t delta) {
    if (rgb.r != other.r) {
        rgb.r += min(max((int16_t)(other.r - rgb.r), (int16_t)(-(int16_t)delta)), (int16_t)delta);
    }
    if (rgb.g != other.g) {
        rgb.g += min(max((int16_t)(other.g - rgb.g), (int16_t)(-(int16_t)delta)), (int16_t)delta);
    }
    if (rgb.b != other.b) {
        rgb.b += min(max((int16_t)(other.b - rgb.b), (int16_t)(-(int16_t)delta)), (int16_t)delta);
    }
}

static uint8_t number_of_flashes             = 0;
static unsigned long time_since_state_change = 0;
static node_state_t last_state               = STATE_DEFAULT;
CRGB baseColor;

void led_task(void *param) {

    unsigned long time;
    while (true) {
        time = millis();

        if (buzzer_color == COLOR_RGB) {
            baseColor = buzzer_color_rgb;
        } else {
            baseColor = colors[buzzer_color];
        }

        if (current_state != last_state) {
            last_state_change = time;
        }
        last_state              = current_state;
        time_since_state_change = time - last_state_change;

        if (low_battery) {
            fill_solid(leds, NUM_LEDS, (millis() / 100) % 2 == 0 ? CRGB(20, 0, 0) : 0);
        } else {

            switch (current_state) {
                case STATE_DEFAULT:
                    get_current_mode()->display();
                    break;
                case STATE_CONFIG:
                    fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(buzzer_color == COLOR_RGB && digitalRead(BUZZER_BUTTON_PIN) == LOW ? 255 : sin8(millis() / 2) / 6 + 40));
                    break;
                case STATE_SHUTDOWN:
                    {
                        get_current_mode()->display();

                        uint8_t leds_to_shutoff = ((float)time_since_state_change / SHUTDOWN_ANIMATION_DURATION) * NUM_LEDS;
                        fill_solid(&leds[(NUM_LEDS - leds_to_shutoff) / 2], leds_to_shutoff, 0);
                    }
                    break;
                case STATE_SHOW_BATTERY:
                    fill_solid(leds, NUM_LEDS, 0);
                    for (uint8_t i = 0; i < NUM_LEDS * battery_percent; i++) {
                        uint8_t rg = i * 255.0f / NUM_LEDS;
                        leds[i]    = CRGB(255 - rg, rg, 0);
                    }

                    if (time_since_state_change > 2000) {
                        set_state(STATE_DEFAULT);
                    }
                    break;
            }
        }

        FastLED.show();
        vTaskDelay(10 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}
