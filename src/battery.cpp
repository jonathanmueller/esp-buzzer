#include <Arduino.h>
#include <RunningAverage.h>
#include <FastLED.h>

#include "comm.h"
#include "battery.h"

/* Interface */
uint32_t battery_voltage = 0;
float battery_percent = 0;
uint8_t battery_percent_rounded = 0;
bool low_battery = false;

/* Defines */
#define BAT_VOLTAGE_PIN D2

#define BAT_VOLTAGE_100_PCT 4200
#define BAT_VOLTAGE_50_PCT 3700
#define BAT_VOLTAGE_0_PCT 3200

#define BAT_VOLTAGE_FILTER_SIZE 16

#define BATTERY_TASK_DURATION 2 // seconds
#define LOW_BATTERY_THRESHOLD 0.01

static RunningAverage battery_running_average(BAT_VOLTAGE_FILTER_SIZE);
uint16_t low_battery_counter = 0;

void measure_battery_voltage() {
    battery_running_average.addValue(2.0 * analogReadMilliVolts(BAT_VOLTAGE_PIN));
    battery_voltage = battery_running_average.getAverage();

    battery_percent = battery_voltage > BAT_VOLTAGE_50_PCT
        ? (0.5 + 0.5 * (battery_voltage - BAT_VOLTAGE_50_PCT) / (BAT_VOLTAGE_100_PCT - BAT_VOLTAGE_50_PCT))
        : (battery_voltage > BAT_VOLTAGE_0_PCT
            ? (0.0 + 0.5 * (battery_voltage - BAT_VOLTAGE_0_PCT) / (BAT_VOLTAGE_50_PCT - BAT_VOLTAGE_0_PCT))
            : 0);
    
    battery_percent_rounded = (uint8_t)(battery_percent*100);
}

/* Task implementation */
void battery_setup() {
    pinMode(BAT_VOLTAGE_PIN, INPUT);
    measure_battery_voltage();
}


void battery_loop() {
    EVERY_N_MILLIS(100) {
        measure_battery_voltage();

        /* Check low battery state */
        if (battery_percent < LOW_BATTERY_THRESHOLD) {
            low_battery_counter++;
            
            if (low_battery_counter > 30) {
                low_battery = true;
            }

            if (low_battery_counter > 60) {
                log_e("Low battery, shutting down.");
                shutdown(true, false);
            }

        } else {
            low_battery_counter = 0;
            low_battery = false;
        }
    }

    EVERY_N_SECONDS(60) { log_d("Battery voltage: %.3fV (%.1f%%)", battery_voltage / 1000.0f, battery_percent*100); }
}

void shutdown(bool turnOffLEDs, bool allowWakeupWithBuzzer) {
    esp_deep_sleep_enable_gpio_wakeup(
        (allowWakeupWithBuzzer ? (1 << BUZZER_BUTTON_PIN) : 0)
        | (1 << BACK_BUTTON_PIN),
        ESP_GPIO_WAKEUP_GPIO_LOW
    );

    current_state = STATE_SHUTDOWN;
    if (turnOffLEDs) {
        FastLED.setBrightness(0);
    }

    delay(30);
    esp_deep_sleep_start();
}