
#include <Arduino.h>
#include "battery.h"
#include "button.h"
#include "espnow.h"

void setup(void)
{
    Serial.begin(9600);
    Serial.println("Creating Tasks...");

    battery_setup();
    espnow_setup();
    button_setup();
}

void loop() {
    battery_loop();
    button_loop();
}