#include <Arduino.h>
#include <FastLED.h>
#include "button.h"
#include "espnow.h"

#define BOOT_BUTTON_PIN D9
#define BUZZER_BUTTON_PIN D0

void button_setup() {
    pinMode(BOOT_BUTTON_PIN, INPUT);
    pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);
}

void button_loop() {
    static bool lastPushedBootButton = false;
    static CEveryNMillis debounceBootButton(50);

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        if (!lastPushedBootButton) {
            broadcast_my_info();
        }
        lastPushedBootButton = true;
        debounceBootButton.reset();
    } else {
        if (debounceBootButton) {
            lastPushedBootButton = false;
        }
    }


    static bool lastPushedBuzzerButton = false;
    static CEveryNMillis debounceBuzzerButton(50);

    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        if (!lastPushedBuzzerButton) {
            Serial.println("BUZZ!");
        }
        lastPushedBuzzerButton = true;
        debounceBuzzerButton.reset();
    } else {
        if (debounceBuzzerButton) {
            lastPushedBuzzerButton = false;
        }
    }
}