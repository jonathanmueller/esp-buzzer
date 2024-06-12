#include <Arduino.h>
#include "led.h"
#include "button.h"
#include "comm.h"
#include "battery.h"
#include "nvm.h"

node_state_t current_state              = STATE_IDLE;
unsigned long last_state_change         = 0;
unsigned long buzzer_active_until       = 0;
unsigned long buzzer_disabled_until     = 0;
unsigned long back_button_pressed_since = 0;
uint16_t both_buttons_pressed_for       = 0;

bool lastPushedBackButton       = false; /* Whether or not the back button was pushed last loop iteration */
bool lastPushedBuzzerButton     = false; /* Whether or not the buzzer button was pushed last loop iteration */
bool pressedBackButtonSinceBoot = false;

void set_state(node_state_t state) {
    current_state     = state;
    last_state_change = millis();
}

void button_setup() {
    // pinMode(BOOT_BUTTON_PIN, INPUT);
    pinMode(BACK_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);

    // pinsBuzzerButtonSinceBoot |= (digitalRead(BUZZER_BUTTON_PIN) == LOW);
    // pressedBootButtonSinceBoot |= (digitalRead(BOOT_BUTTON_PIN) == LOW);
    pressedBackButtonSinceBoot = (digitalRead(BACK_BUTTON_PIN) == LOW);

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
            set_state(STATE_IDLE);
            break;
        }
        yield();
    }

    /* Save to EEPROM */
    nvm_data.color = buzzer_color;
    memcpy(nvm_data.rgb, buzzer_color_rgb.raw, 3);
    nvm_save();
}

static CEveryNMillis buzzStateUpdate(ACCOUNCEMENT_INTERVAL_WHILE_ACTIVE);

void buzz() {
    log_i("BUZZ! Sending state update");

    unsigned long time = millis();
    set_state(STATE_BUZZER_ACTIVE);
    buzzer_active_until = time + nvm_data.game_config.buzzer_active_time;
    send_state_update();
    buzzStateUpdate.reset();
}

void button_loop() {
    unsigned long time = millis();

    if (current_state == STATE_BUZZER_ACTIVE && buzzStateUpdate) {
        send_state_update();
    }

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
            }
            both_buttons_pressed_for   = 0;
            lastPushedBackButton       = false;
            pressedBackButtonSinceBoot = false;
        }
    }

    static CEveryNMillis debounceBuzzerButton(50);

    if (current_state == STATE_BUZZER_ACTIVE && time > buzzer_active_until) {
        log_d("Time's up! On cooldown for a bit.");
        set_state(STATE_DISABLED);
        if (buzzer_disabled_until != -1UL) {
            buzzer_disabled_until = time + nvm_data.game_config.deactivation_time_after_buzzing;
        }
        send_state_update();
    } else if (current_state == STATE_DISABLED && time > buzzer_disabled_until) {
        log_d("Re-enabling.");
        set_state(STATE_IDLE);
        send_state_update();
    }

    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        if (!lastPushedBuzzerButton || !nvm_data.game_config.must_release_before_pressing) {
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
