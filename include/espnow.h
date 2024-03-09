
#ifndef ESPNOW_EXAMPLE_H
#define ESPNOW_EXAMPLE_H

#include "esp_now.h"


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
        
// Send count (1..65535)
// Total count of unicast ESPNOW data to be sent.
#define CONFIG_ESPNOW_SEND_COUNT 50
        
// Send delay (0..65535)
// Delay between sending two ESPNOW data, unit: ms.
#define CONFIG_ESPNOW_SEND_DELAY 500
        
// Send len (10..250)
// Length of ESPNOW data to be sent, unit: byte.
#define CONFIG_ESPNOW_SEND_LEN 10
        
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

#define ESPNOW_QUEUE_SIZE           6


typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} example_espnow_event_id_t;

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
} example_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    example_espnow_event_id_t id;
    example_espnow_event_info_t info;
} espnow_event_t;

enum {
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

enum espnow_data_type_t : uint8_t {
    ESP_DATA_TYPE_HI_I_AM_JOINING_PLEASE_SEND_ALL_YOUR_INFO,
    ESP_DATA_TYPE_HERE_IS_MY_INFO,
    ESP_DATA_TYPE_MAX
};

enum color_t : uint8_t {
    COLOR_RED,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_WHITE,
    COLOR_MAX
} ;

typedef union {
    struct {
        color_t color;
        uint8_t currentState;
    }  __attribute__((packed)) node_info;
    uint8_t raw[0];
}  __attribute__((packed)) espnow_data_playload_t;

/* User defined field of ESPNOW data in this example. */
typedef struct {
    espnow_data_type_t type;
    espnow_data_playload_t payload;
} __attribute__((packed)) example_espnow_data_t;


#ifdef __cplusplus
extern "C" {
#endif

void espnow_setup();
void broadcast_my_info();

#ifdef __cplusplus
}
#endif

#endif