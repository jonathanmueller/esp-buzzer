#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp32-hal-log.h"
#include "comm.h"
#include <WiFi.h>
#include "battery.h"
#include <map>
#include "FastLED.h"
#include "button.h"

#define ESPNOW_MAXDELAY 512
#define ESPNOW_QUEUE_SIZE 10
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

static QueueHandle_t s_comm_queue;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    unsigned long last_seen;
    node_info_t node_info;
} peer_data_t;

static peer_data_t peer_data[ESP_NOW_MAX_TOTAL_PEER_NUM];

static espnow_data_t s_my_broadcast_info = {
    .type = ESP_DATA_TYPE_JOIN_ANNOUNCEMENT,
    .payload = {
        .node_info = { .color = COLOR_RED, .current_state = STATE_IDLE }
    }
};

unsigned long time_of_last_keep_alive_communication = 0;
unsigned long time_of_last_seen_peer = 0;

static esp_now_peer_info_t* malloc_peer_info(const uint8_t *mac) {
    esp_now_peer_info_t* peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        log_e("Malloc peer information fail");
        // vSemaphoreDelete(s_espnow_queue);
        // esp_now_deinit();
        // return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;

    peer->encrypt = false;
    #ifdef CONFIG_ESPNOW_ENCRYPT
    if (!IS_BROADCAST_ADDR(mac)) {
        peer->encrypt = true;
        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
    }
    #else
    #endif

    memcpy(peer->peer_addr, mac, ESP_NOW_ETH_ALEN);

    return peer;
}

static void espnow_deinit();

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    espnow_event_t evt;
    espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        log_e("Send cb arg error");
        return;
    }

    evt.id = ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_comm_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        log_w("Send send queue fail");
    }
}

// static void espnow_recv_cb(const esp_now_recv_info_t * esp_now_info, const uint8_t *data, int len)
static void espnow_recv_cb(const uint8_t *src_addr, const uint8_t *data, int len)
{
    // const uint8_t *src_addr = esp_now_info->src_addr;
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    const uint8_t * mac_addr = src_addr;
    // uint8_t * des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        log_e("Receive cb arg error");
        return;
    }

    evt.id = ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = (uint8_t*)malloc(len);
    if (recv_cb->data == NULL) {
        log_e("Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_comm_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        log_w("Receive queue full. Dropping message.");
        free(recv_cb->data);
    }
}


void send_state_update() {
    unsigned long time = millis();
    s_my_broadcast_info.payload.node_info.battery_percent = battery_percent_rounded;
    s_my_broadcast_info.payload.node_info.current_state = current_state;
    s_my_broadcast_info.payload.node_info.buzzer_active_remaining_ms = current_state == STATE_BUZZER_ACTIVE
        ? (time > buzzer_active_until ? 0 : (buzzer_active_until - time)) 
        : 0;

    if (current_state == STATE_BUZZER_ACTIVE) {
        /* This is notable! Reset shutdown timer */
        time_of_last_keep_alive_communication = time;
        time_of_last_seen_peer = time;
    }

    esp_err_t ret = esp_now_send(s_broadcast_mac, (const uint8_t*)&s_my_broadcast_info, sizeof(s_my_broadcast_info));
    if (ret == ESP_OK) {
        log_d("Broadcasting node information.");
    } else {
        log_e("Send error: %s", esp_err_to_name(ret));
    }
}


static esp_err_t get_peer_info(const uint8_t *mac_addr, peer_data_t *data) {
    if (mac_addr == NULL || data == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }

    for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        if (memcmp(peer_data[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            memcpy(data, &peer_data[i], sizeof(peer_data_t));
            return ESP_OK;
        }
    }

    return false;
}

static esp_err_t add_or_update_peer_info(const uint8_t *mac_addr, peer_data_t *data) {
    if (mac_addr == NULL || data == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }

    peer_data_t *dst = NULL;
    for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        if (memcmp(peer_data[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            dst = &peer_data[i];
            break;
        }
    }

    if (dst == NULL) {
        for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
            if (memcmp(peer_data[i].mac_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
                dst = &(peer_data[i]);
                break;
            }
        }
    }

    if (dst == NULL) {
        return ESP_ERR_ESPNOW_FULL;
    }

    memcpy(dst, data, sizeof(peer_data_t));
    memcpy(dst->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    return ESP_OK;
}

static esp_err_t remove_peer_info(const uint8_t *mac_addr) {
    if (mac_addr == NULL) {
        return ESP_ERR_ESPNOW_ARG;

    }
    for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        if (memcmp(peer_data[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            memset(peer_data[i].mac_addr, 0xFF, ESP_NOW_ETH_ALEN);
            return ESP_OK;
        }
    }

    return ESP_ERR_ESPNOW_NOT_FOUND;
}


static inline void check_stale_peers() {
    unsigned long time = millis();
    bool head = true;
    esp_now_peer_info_t peer;
    peer_data_t peer_data;
    while (esp_now_fetch_peer(head, &peer) == ESP_OK){
        head = false;

        if (get_peer_info(peer.peer_addr, &peer_data) == ESP_OK) {
            unsigned long timeSinceLastSeen = time - peer_data.last_seen;
            if (timeSinceLastSeen > SECONDS_TO_REMEMBER_PEERS * 1000) {
                log_d("Removing peer " MACSTR ", last seen %.1fs ago.", MAC2STR(peer.peer_addr), timeSinceLastSeen / 1000.0f);
                ESP_ERROR_CHECK( esp_now_del_peer(peer.peer_addr) );
                remove_peer_info(peer.peer_addr);
            } else {
                //log_d("Peer: " MACSTR " last seen %.1fs ago, keeping.", MAC2STR(peer.peer_addr), timeSinceLastSeen / 1000.0f);
            }
        }
    }

    esp_now_peer_num_t peer_num;
    ESP_ERROR_CHECK( esp_now_get_peer_num(&peer_num) );
    log_v("Number of known peers is now: %d", peer_num.total_num - 1);
}

static void comm_task(void *pvParameter)
{
    espnow_event_t evt;
    BaseType_t newQueueEntry;

    log_i("Connecting to the node network...");

    s_my_broadcast_info.type = ESP_DATA_TYPE_JOIN_ANNOUNCEMENT;
    send_state_update();
    s_my_broadcast_info.type = ESP_DATA_TYPE_STATE_UPDATE;

    while (true) {
        newQueueEntry = xQueueReceive(s_comm_queue, &evt, 500 / portTICK_RATE_MS);
        unsigned long time = millis();
        if (newQueueEntry == pdTRUE) {
            switch (evt.id) {
                case ESPNOW_SEND_CB:
                {
                    espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                    log_v("Send data to " MACSTR ", status: %s", MAC2STR(send_cb->mac_addr), send_cb->status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
                    break;
                }
                case ESPNOW_RECV_CB:
                {
                    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                    espnow_data_t *data = (espnow_data_t *)recv_cb->data;
                    switch (data->type) {
                        case ESP_DATA_TYPE_JOIN_ANNOUNCEMENT:
                            /* Give them my info */
                            send_state_update();

                            time_of_last_keep_alive_communication = time; // This is a notable event -> reset shutdown timer

                            /* Intentional fallthrough */
                        case ESP_DATA_TYPE_STATE_UPDATE:
                            {
                                time_of_last_seen_peer = time;

                                node_info_t *node_info = &data->payload.node_info;
                                log_v("Task Stack High Water Mark: %d", uxTaskGetStackHighWaterMark(NULL));

                                log_d("Received node state from " MACSTR ": color=%d, currentState=%d, battery=%d%%", MAC2STR(recv_cb->mac_addr), node_info->color, node_info->current_state, node_info->battery_percent);

                                    if ( esp_now_is_peer_exist(recv_cb->mac_addr) == false ) {
                                        /* If MAC address does not exist in peer list, add it to peer list. */
                                        esp_now_peer_info_t *peer = malloc_peer_info(recv_cb->mac_addr);
                                        log_v("Adding peer to list (" MACSTR ").", MAC2STR(peer->peer_addr)); 
                                        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                                        free(peer);
                                    }

                                    peer_data_t peer_data = { .last_seen = time, .node_info = *node_info };
                                    ESP_ERROR_CHECK( add_or_update_peer_info(recv_cb->mac_addr, &peer_data) );

                                // Handle state data
                                if (current_state != STATE_BUZZER_ACTIVE &&
                                    node_info->current_state == STATE_BUZZER_ACTIVE &&
                                    node_info->buzzer_active_remaining_ms > 0) {
                                        if (buzzer_disabled_until < time + node_info->buzzer_active_remaining_ms) {
                                            time_of_last_keep_alive_communication = time; // This is a notable event -> reset shutdown timer

                                            buzzer_disabled_until = time + node_info->buzzer_active_remaining_ms;
                                            current_state = STATE_DISABLED;
                                            log_d("Received buzz from other node. Disabling for %dms", node_info->buzzer_active_remaining_ms);
                                        }                
                                }
                            }
                            break;
                        default:
                            log_v("Unknown data packet received (type=%d)", data->type);
                            break;
                    }
                    
                    free(recv_cb->data);
                    break;
                }
                default:
                    log_e("Callback type error: %d", evt.id);
                    break;
            }
        } else {
            /* No queue entry this time -> timeout */
            EVERY_N_SECONDS(5) { check_stale_peers(); }

            EVERY_N_SECONDS(ACCOUNCEMENT_INTERVAL_SECONDS) { send_state_update(); }

            if (!has_external_power) {
                if (time - time_of_last_keep_alive_communication > SHUTDOWN_TIME_NO_BUZZING) {
                    log_i("Nobody pushing any buttons. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                } else if (time - time_of_last_seen_peer > SHUTDOWN_TIME_NO_COMMS) {
                    log_i("No other buzzer near me. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                }
            }
        }
    }

    vTaskDelete( NULL );
}

static esp_err_t espnow_init(void)
{
    s_comm_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_comm_queue == NULL) {
        log_e("Create mutex fail");
        return ESP_FAIL;
    }

    /* init peer data */
    for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        memset(peer_data[i].mac_addr, 0xFF, ESP_NOW_ETH_ALEN);
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );
    
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc_peer_info(s_broadcast_mac);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    xTaskCreate(&comm_task, "comm_task", 2400, NULL, 5, NULL);

    return ESP_OK;
}

static void espnow_deinit()
{
    vSemaphoreDelete(s_comm_queue);
    esp_now_deinit();
}


/* WiFi should start before using ESPNOW */
void comm_setup(void)
{
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    
    ESP_ERROR_CHECK( esp_wifi_start());

    ESP_ERROR_CHECK( esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif

    espnow_init();
}