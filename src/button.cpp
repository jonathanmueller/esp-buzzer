#include <Arduino.h>
#include "led.h"
#include "button.h"
#include "comm.h"
#include "battery.h"
#include "nvm.h"
#include "bluetooth.h"

unsigned long back_button_pressed_since = 0;
uint16_t both_buttons_pressed_for       = 0;

bool lastPushedBackButton       = false; /* Whether or not the back button was pushed last loop iteration */
bool pressedBackButtonSinceBoot = false;

void button_setup() {
    pinMode(BACK_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);

    pressedBackButtonSinceBoot = (digitalRead(BACK_BUTTON_PIN) == LOW);
}

inline static void next_color() {
    buzzer_color = (color_t)((buzzer_color + 1) % COLOR_NUM);
    if (buzzer_color == COLOR_RGB) {
        buzzer_color = (color_t)((buzzer_color + 1) % COLOR_NUM);
    }
}

void config_loop() {
    log_i("Entering config mode");
    set_state(STATE_CONFIG);

    /* Wait for both buttons to be released */
    while (digitalRead(BUZZER_BUTTON_PIN) == LOW || digitalRead(BACK_BUTTON_PIN) == LOW) {
        yield();
    }
    delay(50);

    while (true) {
        if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
            delay(50);
            bool setToRGB                  = false;
            unsigned long started_pressing = millis();
            while (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
                yield();
                if ((millis() - started_pressing) > 1000) {
                    setToRGB         = true;
                    uint8_t hue      = rgb2hsv_approximate(buzzer_color == COLOR_RGB ? buzzer_color_rgb : colors[buzzer_color]).h;
                    buzzer_color     = COLOR_RGB;
                    buzzer_color_rgb = CHSV(hue, 255, 255);
                    while (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
                        hue++;
                        buzzer_color_rgb = CHSV(hue, 255, 255);
                        delay(50);
                    }
                }
            }
            if (!setToRGB) {
                next_color();
                delay(50);
            }
            delay(50);
        } else if (digitalRead(BACK_BUTTON_PIN) == LOW) {
            set_state(STATE_DEFAULT);
            break;
        }
        yield();
    }

    /* Save to EEPROM */
    nvm_data.color = buzzer_color;
    memcpy(nvm_data.rgb, buzzer_color_rgb.raw, 3);
    nvm_save();
}

void button_loop() {
    unsigned long time = millis();

    static CEveryNMillis debounceBackButtonRelease(50);
    if (digitalRead(BACK_BUTTON_PIN) == LOW) {
        if (!lastPushedBackButton) {
            back_button_pressed_since = time;
            send_state_update();
        }
        lastPushedBackButton = true;
        debounceBackButtonRelease.reset();

        if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
            EVERY_N_MILLIS(100) { both_buttons_pressed_for += 100; }

            if (both_buttons_pressed_for > 2000) {
                config_loop();
            }

            time                      = millis();
            back_button_pressed_since = time; // Reset timer for shutdown if both buttons are pressed
        } else {
            both_buttons_pressed_for = 0;
        }

        if (time - back_button_pressed_since > 3000) {
            /* Shutdown */
            log_d("Shutting down...");
            shutdown(true, false);
        }
    } else {
        if (debounceBackButtonRelease) {
            if (!pressedBackButtonSinceBoot && lastPushedBackButton && (time - back_button_pressed_since) < 1000) {
                set_state(STATE_SHOW_BATTERY);
                bluetooth_set_state(true);
            }
            both_buttons_pressed_for   = 0;
            lastPushedBackButton       = false;
            pressedBackButtonSinceBoot = false;
        }
    }
}
