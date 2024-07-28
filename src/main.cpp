#include <Arduino.h>
#include "battery.h"
#include "button.h"
#include "led.h"
#include "comm.h"
#include "custom_usb.h"
#include "nvm.h"
#include "bluetooth.h"
#include "_config.h"

static RTC_NOINIT_ATTR uint8_t boot_attempts = 0;
void check_safe_mode() {
    boot_attempts++;

    esp_reset_reason_t reason = esp_reset_reason();

    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
        log_i("Reboot reason = %d", reason);
        if (boot_attempts > 5) {
            log_e("Possible reboot loop. Delaying execution...");
            delay(boot_attempts * 1000);
        } else if (reason == ESP_RST_TASK_WDT) {
            /* We might be immediately after flash -> reboot to clear the reason */
            esp_restart();
        }
    } else {
        /* Reset boot attempts */
        boot_attempts = 0;
    }
}

void setup(void) {
    Serial.begin(9600);

    usb_setup();

    check_safe_mode();
    log_i("Starting application...");

    nvm_setup();
    battery_setup();
    button_setup();
    led_setup();

    comm_setup();

    bluetooth_init();
}

void loop() {
    battery_loop();
    button_loop();

    if (boot_attempts > 0 && millis() > 30000) {
        log_d("Boot seems successful.");
        boot_attempts = 0;
    }
}