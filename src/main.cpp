#include <Arduino.h>
#include "battery.h"
#include "button.h"
#include "led.h"
#include "comm.h"
#include "custom_usb.h"
#include "nvm.h"
#include "bluetooth.h"
#include "_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "modes/IMode.h"

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
    mode_setup();

    comm_setup();

    bluetooth_init();
}

#if configUSE_TRACE_FACILITY == 1
static esp_err_t print_real_time_stats(TickType_t xTicksToWait);
#endif

void loop() {
    battery_loop();
    button_loop();
    bluetooth_loop();
    get_current_mode()->loop();

    if (boot_attempts > 0 && millis() > 30000) {
        log_d("Boot seems successful.");
        boot_attempts = 0;
    }

#if configUSE_TRACE_FACILITY == 1
    EVERY_N_SECONDS(10) {
        print_real_time_stats(1000);
    }
#endif
}

#if configUSE_TRACE_FACILITY == 1
#define ARRAY_SIZE_OFFSET               5 // Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE
#define configRUN_TIME_COUNTER_TYPE     TickType_t
#define CONFIG_FREERTOS_NUMBER_OF_CORES 1

static esp_err_t print_real_time_stats(TickType_t xTicksToWait) {
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret;
    uint32_t total_elapsed_time;

    // Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array      = (TaskStatus_t *)malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    // Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array      = (TaskStatus_t *)malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    // Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    // Calculate total_elapsed_time in units of run time stats clock period.
    total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    log_d("| Task             | Run Time | Percentage");
    // Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                // Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle   = NULL;
                break;
            }
        }
        // Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time   = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
            log_d("| %-16s | %8" PRIu32 " | %3" PRIu32 "%%", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    // Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            log_d("| %-16s | Deleted", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            log_d("| %-16s | Created", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit: // Common return path
    free(start_array);
    free(end_array);
    return ret;
}

#endif