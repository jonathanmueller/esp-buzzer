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
    uint8_t mac_addr[ESP_NOW_ETH_ALEN]; // The peer's MAC address
    unsigned long last_seen;            // The last millis() that we received a (non-ping) packet from this peer
    unsigned long last_sent_ping_us;    // Last micros() that we sent a ping to this peer
    uint16_t latency_us;                // The RTT latency in us
    int8_t rssi;                        // The RSSI
    payload_node_info_t node_info;      // The peer's last known node info (i.e. state)
} peer_data_t;

static peer_data_t peer_data[ESP_NOW_MAX_TOTAL_PEER_NUM];

static espnow_data_t s_my_broadcast_info = {
    .type = ESP_DATA_TYPE_JOIN_ANNOUNCEMENT,
    .payload = {
        .node_info = { .color = COLOR_RED, .rgb = { 255, 0, 0 }, .current_state = STATE_IDLE }
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

void send_ping(const uint8_t *mac_addr) {
    espnow_data_t ping = {
        .type = ESP_DATA_TYPE_PING_PONG,
        .payload = {
            .ping_pong = {
                .stage = PING_PONG_STAGE_PING,
                .latency_us = 0,
                .rssi = 0
            }
        }
    };

    peer_data->last_sent_ping_us = micros();
    esp_err_t ret = esp_now_send(mac_addr, (const uint8_t*)&ping, sizeof(ping));
    if (ret == ESP_OK) {
        log_v("Pinging %2x:%2x:%2x:%2x:%2x:%2x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        log_e("Send error: %s", esp_err_to_name(ret));
    }

}

static esp_err_t get_peer_info(const uint8_t *mac_addr, peer_data_t **data) {
    if (mac_addr == NULL || data == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }

    for (uint8_t i = 0; i < ESP_NOW_MAX_TOTAL_PEER_NUM; i++) {
        if (memcmp(peer_data[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            *data = &peer_data[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_ESPNOW_NOT_FOUND;
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
    peer_data_t *peer_data;
    while (esp_now_fetch_peer(head, &peer) == ESP_OK){
        head = false;

        if (get_peer_info(peer.peer_addr, &peer_data) == ESP_OK) {
            unsigned long timeSinceLastSeen = time - peer_data->last_seen;
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

                                payload_node_info_t *node_info = &data->payload.node_info;
                                log_v("Task Stack High Water Mark: %d", uxTaskGetStackHighWaterMark(NULL));

                                log_d("Received node state from " MACSTR ": color=%d, currentState=%d, battery=%d%%", MAC2STR(recv_cb->mac_addr), node_info->color, node_info->current_state, node_info->battery_percent);

                                    if ( esp_now_is_peer_exist(recv_cb->mac_addr) == false ) {
                                        /* If MAC address does not exist in peer list, add it to peer list. */
                                        esp_now_peer_info_t *peer = malloc_peer_info(recv_cb->mac_addr);
                                        log_v("Adding peer to list (" MACSTR ").", MAC2STR(peer->peer_addr)); 
                                        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                                        free(peer);
                                    }

                                    peer_data_t peer_data = { .last_seen = time, .rssi = 0, .node_info = *node_info };
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
                            
                        case ESP_DATA_TYPE_PING_PONG:
                            {
                                if ( esp_now_is_peer_exist(recv_cb->mac_addr) == false ) {
                                    log_d("Ignoring ping from unknown peer");
                                    break;
                                }

                                ping_pong_stage_t stage = data->payload.ping_pong.stage;
                                peer_data_t *peer_data;
                                if (get_peer_info(recv_cb->mac_addr, &peer_data) == ESP_OK) {
                                    unsigned long time_us = micros();
                                    if (stage > PING_PONG_STAGE_PING) {
                                        peer_data->latency_us = min(65535UL, time_us - peer_data->last_sent_ping_us);
                                    }
                                    if (stage < PING_PONG_STAGE_DATA2) {
                                        espnow_data_t pong = {
                                            .type = ESP_DATA_TYPE_PING_PONG,
                                            .payload = {
                                                .ping_pong = {
                                                    .stage = (ping_pong_stage_t)(stage + 1),
                                                    .latency_us = peer_data->latency_us,
                                                    .rssi = peer_data->rssi
                                                }
                                            }
                                        };

                                        esp_err_t ret = esp_now_send(recv_cb->mac_addr, (const uint8_t*)&pong, sizeof(pong));
                                        if (ret != ESP_OK) {
                                            log_e("Send error: %s", esp_err_to_name(ret));
                                        }
                                    }

                                    log_v("%s (stage=%d, latency=%dus, rssi=%ddBm)", (stage%2) == 0 ? "Ping":"Pong", stage, data->payload.ping_pong.latency_us, data->payload.ping_pong.rssi);
                                    peer_data->last_sent_ping_us = time_us;
                                }
                            }
                            break;
                        case ESP_DATA_TYPE_COMMAND:
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
                if (time - time_of_last_keep_alive_communication > (SHUTDOWN_TIME_NO_BUZZING_SECONDS * 1000)) {
                    log_i("Nobody pushing any buttons. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                } else if (time - time_of_last_seen_peer > (SHUTDOWN_TIME_NO_COMMS_SECONDS * 1000)) {
                    log_i("No other buzzer near me. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                }
            }

            #if PING_INTERVAL != 0
                EVERY_N_MILLIS(PING_INTERVAL) {
                    static uint8_t peerToPing = 0;
                    bool head = true;
                    esp_now_peer_info_t peer;
                    peer_data_t *peer_data;
                    uint8_t i = 0;
                    while (esp_now_fetch_peer(head, &peer) == ESP_OK) {
                        head = false;
                        if (i++ < peerToPing) { continue; }

                        if (get_peer_info(peer.peer_addr, &peer_data) == ESP_OK) {
                            send_ping(peer.peer_addr);
                            log_d("Connection info: %2x:%2x:%2x:%2x:%2x:%2x: latency: %dus, rssi=%d", peer.peer_addr[0], peer.peer_addr[1], peer.peer_addr[2], peer.peer_addr[3], peer.peer_addr[4], peer.peer_addr[5], peer_data->latency_us, peer_data->rssi);
                        }
                    }
                    peerToPing++;
                    if (esp_now_fetch_peer(head, &peer) != ESP_OK) {
                        peerToPing = 0;
                    }
                }
            #endif
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

typedef struct
{
  unsigned frame_ctrl : 16;  // 2 bytes / 16 bit fields
  unsigned duration_id : 16; // 2 bytes / 16 bit fields
  uint8_t addr1[6];          // receiver address
  uint8_t addr2[6];          //sender address
  uint8_t addr3[6];          // filtering address
  unsigned sequence_ctrl : 16; // 2 bytes / 16 bit fields
} wifi_ieee80211_mac_hdr_t;    // 24 bytes

typedef struct
{
  wifi_ieee80211_mac_hdr_t hdr;
  unsigned category_code : 8; // 1 byte / 8 bit fields
  uint8_t oui[3]; // 3 bytes / 24 bit fields
  uint8_t payload[0];
} wifi_ieee80211_packet_t;

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
    if (type != WIFI_PKT_MGMT)
        return;

    static const uint8_t ACTION_SUBTYPE = 0xd0;
    static const uint8_t ESPRESSIF_OUI[] = {0x18, 0xfe, 0x34};
    static peer_data_t *peer_data;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    // Only continue processing if this is an action frame containing the Espressif OUI.
    if ((ACTION_SUBTYPE == (hdr->frame_ctrl & 0xFF)) && memcmp(ipkt->oui, ESPRESSIF_OUI, 3) == 0) {
        if (get_peer_info(hdr->addr2, &peer_data) == ESP_OK) {
            peer_data->rssi = ppkt->rx_ctrl.rssi;
            log_v("Packet from %2x:%2x:%2x:%2x:%2x:%2x: RSSI = %ddBm", hdr->addr2[0], hdr->addr2[1], hdr->addr2[2], hdr->addr2[3], hdr->addr2[4], hdr->addr2[5], ppkt->rx_ctrl.rssi);
        }
    }
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

    ESP_ERROR_CHECK( esp_wifi_set_promiscuous(true) );
    
    ESP_ERROR_CHECK( esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb) );

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif

    espnow_init();
}