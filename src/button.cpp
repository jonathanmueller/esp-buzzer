#include <Arduino.h>
#include "led.h"
#include "button.h"
#include "comm.h"
#include "battery.h"
#include "nvm.h"

node_state_t current_state              = STATE_IDLE;
unsigned long buzzer_active_until       = 0;
unsigned long buzzer_disabled_until     = 0;
unsigned long back_button_pressed_since = 0;
uint16_t both_buttons_pressed_for       = 0;

bool lastPushedBootButton   = false; /* Whether or not the boot button was pushed last loop iteration */
bool lastPushedBuzzerButton = false; /* Whether or not the buzzer button was pushed last loop iteration */
uint8_t startup_pins        = 0;

void button_setup() {
    pinMode(BOOT_BUTTON_PIN, INPUT);
    pinMode(BACK_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);

    startup_pins |= (digitalRead(BUZZER_BUTTON_PIN) == LOW) << 0;
    startup_pins |= (digitalRead(BACK_BUTTON_PIN) == LOW) << 1;
    startup_pins |= (digitalRead(BOOT_BUTTON_PIN) == LOW) << 2;

    // if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
    //     lastPushedBuzzerButton = true;
    // }
}

inline static void next_color() {
    buzzer_color = (color_t)((buzzer_color + 1) % COLOR_NUM);
    if (buzzer_color == COLOR_RGB) {
        buzzer_color = (color_t)((buzzer_color + 1) % COLOR_NUM);
    }
}

void config_loop() {
    log_i("Entering config mode");
    current_state = STATE_CONFIG;

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
            current_state = STATE_IDLE;
            break;
        }
        yield();
    }

    /* Save to EEPROM */
    nvm_data.color = buzzer_color;
    memcpy(nvm_data.rgb, buzzer_color_rgb.raw, 3);
    nvm_save();
}

void buzz() {
    log_i("BUZZ! Sending state update");

    unsigned long time  = millis();
    current_state       = STATE_BUZZER_ACTIVE;
    buzzer_active_until = time + BUZZER_ACTIVE_TIME;
    send_state_update();
}

void button_loop() {
    unsigned long time = millis();

    static CEveryNMillis debounceBackButtonRelease(50);
    if (digitalRead(BACK_BUTTON_PIN) == LOW) {
        if (!lastPushedBootButton) {
            back_button_pressed_since = time;
            send_state_update();
        }
        lastPushedBootButton = true;
        debounceBackButtonRelease.reset();

        if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
            EVERY_N_MILLIS(100) { both_buttons_pressed_for += 100; }

            if (both_buttons_pressed_for > 2000) {
                config_loop();
                current_state = STATE_SHOW_BATTERY;
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
            both_buttons_pressed_for = 0;
            lastPushedBootButton     = false;
        }
    }

    static CEveryNMillis debounceBuzzerButton(50);

    if (current_state == STATE_BUZZER_ACTIVE && time > buzzer_active_until) {
        log_d("Time's up! On cooldown for a bit.");
        current_state = STATE_DISABLED;
        if (buzzer_disabled_until != -1UL) {
            buzzer_disabled_until = time + BUZZER_DISABLED_TIME;
        }
        send_state_update();
    } else if (current_state == STATE_DISABLED && time > buzzer_disabled_until) {
        log_d("Re-enabling.");
        current_state = STATE_IDLE;
        send_state_update();
    }

    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        if (!lastPushedBuzzerButton) {
            if (current_state == STATE_IDLE) {
                buzz();
            }
        }
        lastPushedBuzzerButton = true;
        debounceBuzzerButton.reset();
    } else {
        if (debounceBuzzerButton) {
            lastPushedBuzzerButton = false;
        }
    }
}
