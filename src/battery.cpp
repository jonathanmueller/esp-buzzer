#include <Arduino.h>
#include <RunningAverage.h>
#include <FastLED.h>

#include "battery.h"

/* Interface */
uint32_t battery_voltage = 0;
float battery_percent = 0;


/* Defines */
#define BAT_VOLTAGE_PIN D2

#define BAT_VOLTAGE_100_PCT 4200
#define BAT_VOLTAGE_50_PCT 3700
#define BAT_VOLTAGE_0_PCT 3200

#define BAT_VOLTAGE_FILTER_SIZE 16

#define BATTERY_TASK_DURATION 2 // seconds

static RunningAverage battery_running_average(BAT_VOLTAGE_FILTER_SIZE);


/* Task implementation */
void battery_setup() {
    pinMode(BAT_VOLTAGE_PIN, INPUT);
}

void battery_loop() {
    battery_running_average.addValue(2.0 * analogReadMilliVolts(BAT_VOLTAGE_PIN));
    battery_voltage = battery_running_average.getAverage();

    battery_percent = battery_voltage > BAT_VOLTAGE_50_PCT
        ? (0.5 + 0.5 * (battery_voltage - BAT_VOLTAGE_50_PCT) / (BAT_VOLTAGE_100_PCT - BAT_VOLTAGE_50_PCT))
        : (battery_voltage > BAT_VOLTAGE_0_PCT
            ? (0.0 + 0.5 * (battery_voltage - BAT_VOLTAGE_0_PCT) / (BAT_VOLTAGE_50_PCT - BAT_VOLTAGE_0_PCT))
            : 0);

    EVERY_N_SECONDS(60) {
        Serial.print("Battery voltage: ");
        Serial.print(battery_voltage / 1000.0f, 3);
        Serial.print("V (");
        Serial.print(battery_percent*100, 2);
        Serial.println("%)");
    }
}