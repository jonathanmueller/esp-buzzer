#include <Arduino.h>

#define FASTLED_INTERNAL
#include <FastLED.h>

#include "comm.h"
#include "battery.h"
#include "_config.h"

/* Interface */
uint32_t battery_voltage        = 0;
float battery_percent           = 0;
uint8_t battery_percent_rounded = 0;
bool low_battery                = false;
bool has_external_power         = false;

// static RunningAverage battery_running_average(BAT_VOLTAGE_FILTER_SIZE);
static struct {
    uint32_t data[BAT_VOLTAGE_FILTER_SIZE] = { 0 };
    uint8_t pointer                        = 0;
    uint8_t fill                           = 0;
    float average                          = 0;
} battery_average;

uint16_t low_battery_counter = 0;

void measure_battery_voltage() {
    /* Average measurements */
    battery_average.data[battery_average.pointer] = 2.0 * analogReadMilliVolts(BAT_VOLTAGE_PIN);
    battery_average.pointer                       = (battery_average.pointer + 1) % BAT_VOLTAGE_FILTER_SIZE;
    if (battery_average.fill < BAT_VOLTAGE_FILTER_SIZE) {
        battery_average.fill++;
    }
    battery_average.average = 0;
    if (battery_average.fill > 0) {
        for (uint8_t i = 0; i < battery_average.fill; i++) {
            battery_average.average += battery_average.data[i];
        }
        battery_average.average /= battery_average.fill;
    }

    battery_voltage = battery_average.average;

    battery_percent = battery_voltage > BAT_VOLTAGE_100_PCT ? 1
                                                            : (battery_voltage > BAT_VOLTAGE_50_PCT
                                                                   ? (0.5 + 0.5 * (battery_voltage - BAT_VOLTAGE_50_PCT) / (BAT_VOLTAGE_100_PCT - BAT_VOLTAGE_50_PCT))
                                                                   : (battery_voltage > BAT_VOLTAGE_0_PCT
                                                                          ? (0.0 + 0.5 * (battery_voltage - BAT_VOLTAGE_0_PCT) / (BAT_VOLTAGE_50_PCT - BAT_VOLTAGE_0_PCT))
                                                                          : 0));

    battery_percent_rounded = (uint8_t)(battery_percent * 100);

    has_external_power = (battery_voltage > BAT_VOLTAGE_EXTERNAL_POWER);
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
            low_battery         = false;
        }
    }

    EVERY_N_SECONDS(60) { log_d("Battery voltage: %.3fV (%.1f%%)", battery_voltage / 1000.0f, battery_percent * 100); }
}

void shutdown(bool turnOffLEDs, bool allowWakeupWithBuzzer) {
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    esp_deep_sleep_enable_gpio_wakeup(
        (allowWakeupWithBuzzer ? (1 << BUZZER_BUTTON_PIN) : 0) | (esp_sleep_is_valid_wakeup_gpio((gpio_num_t)BACK_BUTTON_PIN) ? (1 << BACK_BUTTON_PIN) : 0),
        ESP_GPIO_WAKEUP_GPIO_LOW);

    set_state(STATE_SHUTDOWN);
    delay(SHUTDOWN_ANIMATION_DURATION);
    if (turnOffLEDs) {
        FastLED.setBrightness(0);
    }

    while ((allowWakeupWithBuzzer && digitalRead(BUZZER_BUTTON_PIN) == LOW) || digitalRead(BACK_BUTTON_PIN) == LOW) {
        yield();
    }
    delay(50);

    esp_deep_sleep_start();
#else
    esp_restart();
#endif
}