#include <Arduino.h>
#include "led.h"
#include "button.h"
#include "comm.h"
#include "battery.h"


node_state_t current_state = STATE_IDLE;
unsigned long buzzer_active_until = 0;
unsigned long buzzer_disabled_until = 0;
uint16_t back_button_pressed_since = 0;

bool lastPushedBootButton = false;  /* Whether or not the boot button was pushed last loop iteration */
bool lastPushedBuzzerButton = false;  /* Whether or not the buzzer button was pushed last loop iteration */

void button_setup() {
    pinMode(BOOT_BUTTON_PIN, INPUT);
    pinMode(BACK_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);

    // if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
    //     lastPushedBuzzerButton = true;
    // }
}

void button_loop() {
    unsigned long time = millis();

    static CEveryNMillis debounceBootButton(50);
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        if (!lastPushedBootButton) {
            back_button_pressed_since = time;
            send_state_update();
        }
        lastPushedBootButton = true;
        debounceBootButton.reset();

        if (time - back_button_pressed_since > 5000) {
            /* Shutdown */
            log_d("Shutting down...");
            shutdown(true, false);
        }


    } else {
        if (debounceBootButton) {
            lastPushedBootButton = false;
        }
    }


    static CEveryNMillis debounceBuzzerButton(50);

    if (current_state == STATE_BUZZER_ACTIVE && time > buzzer_active_until) {
        log_d("Time's up! On cooldown for a bit.");
        current_state = STATE_DISABLED;
        buzzer_disabled_until = time + BUZZER_DISABLED_TIME;
    } else if (current_state == STATE_DISABLED && time > buzzer_disabled_until) {
        log_d("Re-enabling.");
        current_state = STATE_IDLE;
    }

    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        if (!lastPushedBuzzerButton) {
            if (current_state == STATE_IDLE) {
                log_i("BUZZ! Sending state update");
                
                current_state = STATE_BUZZER_ACTIVE;
                buzzer_active_until = time + BUZZER_ACTIVE_TIME;
                send_state_update();
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