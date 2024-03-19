
#pragma once

#include "button.h"
#include "esp_now.h"
#include "led.h"

#define SECONDS_TO_REMEMBER_PEERS 30
#define ACCOUNCEMENT_INTERVAL_SECONDS 10
#define SHUTDOWN_TIME_NO_BUZZING    (1000 * 60 * 20)    // 20 minutes without buzzing, even when others are around -> shut down
#define SHUTDOWN_TIME_NO_COMMS      (1000 * 60 * 5)     // 5 minutes without another nearby buzzer -> shutdown

#undef CONFIG_ESPNOW_ENCRYPT

// ESPNOW primary master key
// ESPNOW primary master for the example to use. The length of ESPNOW primary master must be 16 bytes.
#define CONFIG_ESPNOW_PMK "pmk1234567890123"
        
// ESPNOW local master key
// ESPNOW local master for the example to use. The length of ESPNOW local master must be 16 bytes.
#define CONFIG_ESPNOW_LMK "lmk1234567890123"
        
// Channel (0..14)
// The channel on which sending and receiving ESPNOW data.
#define CONFIG_ESPNOW_CHANNEL 1
        
// Enable Long Range
// When enable long range, the PHY rate of ESP32 will be 512Kbps or 256Kbps
#define CONFIG_ESPNOW_ENABLE_LONG_RANGE 0
        
#if CONFIG_ESPNOW_WIFI_MODE_STATION
// Enable ESPNOW Power Save
// With ESPNOW power save enabled, chip would be able to wakeup and sleep periodically
// Notice ESP_WIFI_STA_DISCONNECTED_PM_ENABLE is essential at Wi-Fi disconnected
  #define CONFIG_ESPNOW_ENABLE_POWER_SAVE 0
        
  #if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    // ESPNOW wake window, unit in millisecond (0..65535)
    #define CONFIG_ESPNOW_WAKE_WINDOW 50
        
    // ESPNOW wake interval, unit in millisecond (1..65535)
    #define CONFIG_ESPNOW_WAKE_INTERVAL 100
  #endif
#endif

//////////////////////////////////////////////////////////////////////////////////////

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF  WIFI_IF_STA

typedef enum {
    ESPNOW_SEND_CB,
    ESPNOW_RECV_CB,
} espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_recv_cb_t;

typedef union {
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    espnow_event_id_t id;
    espnow_event_info_t info;
} espnow_event_t;

enum {
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

enum espnow_data_type_t : uint8_t {
    ESP_DATA_TYPE_JOIN_ANNOUNCEMENT,
    ESP_DATA_TYPE_STATE_UPDATE,
    ESP_DATA_TYPE_MY_BUZZER_WAS_PRESSED,
    ESP_DATA_TYPE_MAX
};

typedef struct {
    uint8_t battery_percent;
    color_t color;
    node_state_t current_state;
    uint16_t buzzer_active_remaining_ms;
}  __attribute__((packed)) node_info_t;

typedef union {
    node_info_t node_info;
    uint8_t raw[0];
}  __attribute__((packed)) espnow_data_playload_t;

/* User defined field of ESPNOW data in this example. */
typedef struct {
    espnow_data_type_t type;
    espnow_data_playload_t payload;
} __attribute__((packed)) espnow_data_t;


#ifdef __cplusplus
extern "C" {
#endif

void comm_setup();
void send_state_update();

#ifdef __cplusplus
}
#endif
