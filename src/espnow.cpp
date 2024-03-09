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
#include "espnow.h"
#include <WiFi.h>


#define ESPNOW_MAXDELAY 512
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

static QueueHandle_t s_espnow_queue;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };


static example_espnow_data_t s_my_broadcast_info = {
    .type = ESP_DATA_TYPE_HI_I_AM_JOINING_PLEASE_SEND_ALL_YOUR_INFO,
    .payload = {
        .node_info = { .color = COLOR_RED, .currentState = 0 }
    }
};


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

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        log_w("Send send queue fail");
    }
}

static void espnow_recv_cb(const uint8_t *src_addr, const uint8_t *data, int len)
{
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    const uint8_t * mac_addr = src_addr;
    // uint8_t * des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        log_e("Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = (uint8_t*)malloc(len);
    if (recv_cb->data == NULL) {
        log_e("Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        log_w("Receive queue full. Dropping message.");
        free(recv_cb->data);
    }
}

void broadcast_my_info() {
    esp_err_t ret = esp_now_send(s_broadcast_mac, (const uint8_t*)&s_my_broadcast_info, sizeof(s_my_broadcast_info));
    if (ret != ESP_OK) {
        log_e("Send error: %s", esp_err_to_name(ret));
        // example_espnow_deinit(send_param);
        // vTaskDelete(NULL);
    }
}

static void espnow_task(void *pvParameter)
{
    espnow_event_t evt;

    vTaskDelay(3000 / portTICK_RATE_MS);
    log_i("Connecting to the node network...");

    s_my_broadcast_info.type = ESP_DATA_TYPE_HI_I_AM_JOINING_PLEASE_SEND_ALL_YOUR_INFO;
    broadcast_my_info();
    s_my_broadcast_info.type = ESP_DATA_TYPE_HERE_IS_MY_INFO;

    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case EXAMPLE_ESPNOW_SEND_CB:
            {
                espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                log_d("Send data to " MACSTR ", status1: %s", MAC2STR(send_cb->mac_addr), send_cb->status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
                break;
            }
            case EXAMPLE_ESPNOW_RECV_CB:
            {
                espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                example_espnow_data_t *data = (example_espnow_data_t *)recv_cb->data;
                switch (data->type) {
                    case ESP_DATA_TYPE_HI_I_AM_JOINING_PLEASE_SEND_ALL_YOUR_INFO:
                        /* Give them my info */
                        broadcast_my_info();

                        /* Intentional fallthrough */
                    case ESP_DATA_TYPE_HERE_IS_MY_INFO:
                        log_i("Received join announcement from: " MACSTR "", MAC2STR(recv_cb->mac_addr));
                        log_i("Node info: color=%d, currentState=%d", data->payload.node_info.color, data->payload.node_info.currentState);

                        /* If MAC address does not exist in peer list, add it to peer list. */
                        if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                            esp_now_peer_info_t *peer = malloc_peer_info(recv_cb->mac_addr);
                            log_d("Adding peer to list (" MACSTR ").", MAC2STR(peer->peer_addr)); 
                            ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                            free(peer);
                        }

                        esp_now_peer_num_t peer_num;
                        ESP_ERROR_CHECK( esp_now_get_peer_num(&peer_num) );
                        log_i("Number of known peers is now: %d", peer_num.total_num - 1);
                        {
                            bool head = true;
                            esp_now_peer_info_t peer;
                            while (esp_now_fetch_peer(head, &peer) == ESP_OK){
                                head = false;
                                log_d("Peer: " MACSTR "", MAC2STR(peer.peer_addr));
                            }
                        }
                        
                        break;
                    default:
                        log_d("Unknown data packet received (type=%d)", data->type);
                        break;
                }
                
                free(recv_cb->data);
                break;
            }
            default:
                log_e("Callback type error: %d", evt.id);
                break;
        }
    }
}

static esp_err_t espnow_init(void)
{
    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL) {
        log_e("Create mutex fail");
        return ESP_FAIL;
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

    xTaskCreate(&espnow_task, "espnow_task", 2048, NULL, 5, NULL);

    return ESP_OK;
}

static void espnow_deinit()
{
    vSemaphoreDelete(s_espnow_queue);
    esp_now_deinit();
}


/* WiFi should start before using ESPNOW */
void espnow_setup(void)
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