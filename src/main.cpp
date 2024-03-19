#include <Arduino.h>
#include "battery.h"
#include "button.h"
#include "led.h"
#include "comm.h"
#include "custom_usb.h"
#include "nvm.h"

void check_safe_mode() {
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT ) {
        log_e("Unexpected reboot (reason = %d). Delaying execution...", reason);
        delay(5000);
    }
}


void setup(void)
{
    Serial.begin(9600);
    check_safe_mode();
    
    log_i("Starting application...");
    
    usb_setup();

    nvm_setup();
    battery_setup();
    comm_setup();
    button_setup();
    led_setup();
}

void loop() {
    battery_loop();
    button_loop();
}