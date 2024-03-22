
#pragma once

#include "button.h"
#include "esp_now.h"
#include "led.h"
#include "_config.h"

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

enum espnow_data_type_t : uint8_t {
    ESP_DATA_TYPE_JOIN_ANNOUNCEMENT,        /* payload type: payload_node_info_t */
    ESP_DATA_TYPE_STATE_UPDATE,             /* payload type: payload_node_info_t */
    ESP_DATA_TYPE_PING_PONG,                /* payload type: payload_ping_pong_t */
    ESP_DATA_TYPE_COMMAND,
    ESP_DATA_TYPE_MAX
};

enum node_type_t : uint8_t {
    NODE_TYPE_BUZZER = 0,
    NODE_TYPE_CONTROLLER = 1
};


typedef struct {
    node_type_t node_type;
    uint8_t battery_percent;
    color_t color;
    uint8_t rgb[3];
    node_state_t current_state;
    uint16_t buzzer_active_remaining_ms;
}  __attribute__((packed)) payload_node_info_t;

enum ping_pong_stage_t : uint8_t {
    PING_PONG_STAGE_PING,
    PING_PONG_STAGE_PONG,
    PING_PONG_STAGE_DATA1,
    PING_PONG_STAGE_DATA2
};

typedef struct {
    ping_pong_stage_t stage;               /* 0: ping, 1: pong, 2: data1, 3: data2 */
    uint16_t latency_us;                   /* measured latency (data1: ping to pong, data2: pong to data1) */
    int8_t rssi;                           /* measured latency of last packet */
}  __attribute__((packed)) payload_ping_pong_t;

typedef union {
    payload_node_info_t node_info;
    payload_ping_pong_t ping_pong;
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
