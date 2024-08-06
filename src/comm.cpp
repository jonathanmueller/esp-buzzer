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
#include "button.h"
#include <nvm.h>
#include "bluetooth.h"
#include "modes/ModeDefault.h"

#define FASTLED_INTERNAL
#include <FastLED.h>

#define ESPNOW_MAXDELAY         512
#define ESPNOW_QUEUE_SIZE       10
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

static QueueHandle_t s_comm_queue;

uint8_t comm_task_started                 = false;
uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t my_mac_addr[ESP_NOW_ETH_ALEN]     = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

uint16_t pingInterval = DEFAULT_PING_INTERVAL;

peer_data_t peer_data_table[PEER_DATA_TABLE_ENTRIES];

static espnow_data_t s_my_broadcast_info = {
    .type    = ESP_DATA_TYPE_JOIN_ANNOUNCEMENT,
    .payload = {
        .node_info = {
            .version            = VERSION_CODE,
            .color              = COLOR_RED,
            .rgb                = { 255, 0, 0 },
            .current_state      = STATE_DEFAULT,
            .current_mode       = MODE_DEFAULT,
            .current_mode_state = { .node_state_default = MODE_DEFAULT_STATE_IDLE },
        } }
};

unsigned long time_of_last_keep_alive_communication = 0;
unsigned long time_of_last_seen_peer                = 0;

static esp_now_peer_info_t *malloc_peer_info(const uint8_t *mac) {
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        log_e("Malloc peer information fail");
        // vSemaphoreDelete(s_espnow_queue);
        // esp_now_deinit();
        // return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx   = ESPNOW_WIFI_IF;

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
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
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
static void espnow_recv_cb(const uint8_t *src_addr, const uint8_t *data, int len) {
    // const uint8_t *src_addr = esp_now_info->src_addr;
    espnow_event_t evt;
    espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    const uint8_t *mac_addr         = src_addr;
    // uint8_t * des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        log_e("Receive cb arg error");
        return;
    }

    evt.id = ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = (uint8_t *)malloc(len);
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

void reset_shutdown_timer() {
    unsigned long time                    = millis();
    time_of_last_keep_alive_communication = time;
    time_of_last_seen_peer                = time;
}

void update_my_info() {
    unsigned long time                    = millis();
    s_my_broadcast_info.payload.node_info = {
        .version            = VERSION_CODE,
        .node_type          = has_external_power ? NODE_TYPE_CONTROLLER : NODE_TYPE_BUZZER,
        .battery_percent    = battery_percent_rounded,
        .battery_voltage    = battery_voltage,
        .color              = buzzer_color,
        .rgb                = { buzzer_color_rgb.r, buzzer_color_rgb.g, buzzer_color_rgb.b },
        .key_config         = nvm_data.key_config,
        .current_state      = current_state,
        .current_mode       = nvm_data.mode,
        .current_mode_state = get_current_mode()->getState(),
    };

    get_current_mode()->update_my_info(&s_my_broadcast_info.payload.node_info);
    /* If we're not a controller, the first peer is ourself, otherwise, return */
    if (has_external_power) { return; }

    peer_data_table[0].last_seen         = time;
    peer_data_table[0].last_sent_ping_us = micros();
    peer_data_table[0].node_info         = s_my_broadcast_info.payload.node_info;
}

void send_state_update() {
    update_my_info();

    esp_err_t ret = esp_now_send(s_broadcast_mac, (const uint8_t *)&s_my_broadcast_info, sizeof(s_my_broadcast_info));
    if (ret == ESP_OK) {
        log_d("Broadcasting node information.");
    } else {
        log_e("Send error: %s", esp_err_to_name(ret));
    }
}

static esp_err_t get_peer_info(const uint8_t *mac_addr, peer_data_t **data) {
    if (mac_addr == NULL || data == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }

    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(peer_data_table[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            *data = &peer_data_table[i];
            return ESP_OK;
        }
    }

    return ESP_ERR_ESPNOW_NOT_FOUND;
}

static esp_err_t get_or_create_peer_info(const uint8_t *mac_addr, peer_data_t **data) {
    if (mac_addr == NULL || data == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }

    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(peer_data_table[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            *data = &peer_data_table[i];
            return ESP_OK;
        }
    }

    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(peer_data_table[i].mac_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            memset(&peer_data_table[i], 0, sizeof(peer_data_t));
            memcpy(&peer_data_table[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

            *data = &(peer_data_table[i]);
            return ESP_OK;
        }
    }

    return ESP_ERR_ESPNOW_FULL;
}

static esp_err_t remove_peer_info(const uint8_t *mac_addr) {
    if (mac_addr == NULL) {
        return ESP_ERR_ESPNOW_ARG;
    }
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(peer_data_table[i].mac_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            memset(peer_data_table[i].mac_addr, 0xFF, ESP_NOW_ETH_ALEN);
            return ESP_OK;
        }
    }

    return ESP_ERR_ESPNOW_NOT_FOUND;
}

void send_ping(const uint8_t *mac_addr) {
    espnow_data_t ping = {
        .type    = ESP_DATA_TYPE_PING_PONG,
        .payload = {
            .ping_pong = {
                .stage      = PING_PONG_STAGE_PING,
                .latency_us = 0,
                .rssi       = 0,
            },
        },
    };

    peer_data_t *peer_data;
    ESP_ERROR_CHECK(get_or_create_peer_info(mac_addr, &peer_data));
    peer_data->last_sent_ping_us = micros();

    esp_err_t ret = esp_now_send(mac_addr, (const uint8_t *)&ping, sizeof(ping));
    if (ret == ESP_OK) {
        log_v("Pinging %2x:%2x:%2x:%2x:%2x:%2x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        log_e("Send error: %s", esp_err_to_name(ret));
    }
}

void cleanup_peer_list() {
    if (!comm_task_started) { return; }

    bool peer_list_updated = false;

    unsigned long time = millis();
    {
        bool head = true;

        esp_now_peer_info_t peer;
        peer_data_t *peer_data;
        while (esp_now_fetch_peer(head, &peer) == ESP_OK) {
            head = false;

            if (get_peer_info(peer.peer_addr, &peer_data) == ESP_OK) {
                unsigned long timeSinceLastSeen = time - peer_data->last_seen;
                if (timeSinceLastSeen > SECONDS_TO_REMEMBER_PEERS * 1000) {
                    log_d("Removing peer " MACSTR ", last seen %.1fs ago.", MAC2STR(peer.peer_addr), timeSinceLastSeen / 1000.0f);
                    ESP_ERROR_CHECK(esp_now_del_peer(peer.peer_addr));
                    remove_peer_info(peer.peer_addr);
                    peer_list_updated = true;
                } else {
                    // log_d("Peer: " MACSTR " last seen %.1fs ago, keeping.", MAC2STR(peer.peer_addr), timeSinceLastSeen / 1000.0f);
                }
            }
        }
    }

    for (uint8_t i = 0; i < node_mode_t::NUM_MODES; i++) {
        IMode *mode = get_mode((node_mode_t)i);
        for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
            /* Give each mode a chance to clean up peer state */
            if (mode->cleanup_peer_data(&peer_data_table[i])) {
                peer_list_updated = true;
            }
        }
    }

    {
        esp_now_peer_num_t peer_num;
        ESP_ERROR_CHECK(esp_now_get_peer_num(&peer_num));
        log_v("Number of known peers is now: %d", peer_num.total_num - 1);
    }

    if (peer_list_updated) {
        bluetooth_notify_peer_list_changed();
    }
}

boolean executeCommand(uint8_t mac_addr[6], payload_command_t *command, uint32_t len) {
    if (mac_addr != NULL &&
        (mac_addr[0] != 0 ||
         mac_addr[0] != 0 ||
         mac_addr[0] != 0 ||
         mac_addr[0] != 0 ||
         mac_addr[0] != 0 ||
         mac_addr[0] != 0) &&
        0 != memcmp(mac_addr, my_mac_addr, ESP_NOW_ETH_ALEN)) {
        /* Don't execute here, but on peer node */
        log_v("Received a command for another peer.");

        espnow_data_t relayed_command;
        relayed_command.type = ESP_DATA_TYPE_COMMAND;

        memcpy(&relayed_command.payload.command, command, len);

        esp_err_t ret = esp_now_send(mac_addr, (uint8_t *)&relayed_command, sizeof(relayed_command));
        if (ret == ESP_OK) {
            log_d("Relaying command...");
        } else {
            log_e("Send error: %s", esp_err_to_name(ret));
        }

        return true;
    }

    log_v("Received a command for me.");

    /* Execute command */
    switch (command->command) {
        case COMMAND_SET_COLOR:
            {
                color_t color = command->args.set_color.color;
                if (color == COLOR_RGB) {
                    nvm_data.color = color;
                    memcpy(nvm_data.rgb, command->args.set_color.rgb, 3);

                    buzzer_color     = nvm_data.color;
                    buzzer_color_rgb = CRGB(nvm_data.rgb[0], nvm_data.rgb[1], nvm_data.rgb[2]);

                    nvm_save();
                    send_state_update();
                    return true;
                } else if (color < COLOR_NUM) {
                    nvm_data.color = color;
                    buzzer_color   = nvm_data.color;
                    nvm_save();
                    send_state_update();
                    return true;
                }
                return false;
            }
            break;
        case COMMAND_SET_GAME_CONFIG:
            {
                game_config_t *game_config = &command->args.game_config;
                uint16_t crc               = esp_rom_crc16_be(0, (const uint8_t *)game_config, (const uint8_t *)&game_config->crc - (const uint8_t *)game_config);
                if (crc != game_config->crc) {
                    log_e("Received invalid game config (%lu), (CRC mismatch %2x vs %2x). Ignoring", (const uint8_t *)&game_config->crc - (const uint8_t *)game_config, crc, game_config->crc);
                    return false;
                } else {
                    log_d("Received game config update.");
                    nvm_data.game_config = *game_config;
                    nvm_save();
                    return true;
                }
            }
            break;
        case COMMAND_SET_KEY_CONFIG:
            {
                key_config_t *key_config = &command->args.key_config;
                log_d("Received key config update.");
                nvm_data.key_config = *key_config;
                nvm_save();
                send_state_update();
                return true;
            }
            break;
        case COMMAND_SET_MODE:
            {
                node_mode_t mode = command->args.mode;
                if (mode < node_mode_t::NUM_MODES) {
                    log_d("Changing mode.");
                    set_mode(mode);
                    nvm_data.mode = mode;
                    nvm_save();
                    send_state_update();
                } else {
                    log_e("Received invalid mode %d", mode);
                }
                return true;
            }
            break;
        case COMMAND_BUZZ:
            modeDefault.buzz();
            return true;
        case COMMAND_SET_INACTIVE:
            modeDefault.setActive(true);
            send_state_update();
            return true;
        case COMMAND_SET_ACTIVE:
            modeDefault.setActive(false);
            send_state_update();
            return true;
        case COMMAND_RESET:
            log_d("Received restart command.");
            esp_restart();
            return true;
        case COMMAND_SHUTDOWN:
            log_d("Received shutdown command.");
            shutdown(true, true);
            return true;
        default:
            log_d("Unknown command received (id=%d)", command->command);
            break;
    }
    return false;
}

static void comm_task(void *pvParameter) {
    espnow_event_t evt;
    BaseType_t newQueueEntry;

    log_i("Connecting to the node network...");

    s_my_broadcast_info.type = ESP_DATA_TYPE_JOIN_ANNOUNCEMENT;
    send_state_update();
    s_my_broadcast_info.type = ESP_DATA_TYPE_STATE_UPDATE;

    comm_task_started = true;

    while (true) {
        newQueueEntry      = xQueueReceive(s_comm_queue, &evt, 500 / portTICK_RATE_MS);
        unsigned long time = millis();
        if (newQueueEntry == pdTRUE) {
            switch (evt.id) {
                case ESPNOW_SEND_CB:
                    {
                        espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                        if (send_cb->status == ESP_NOW_SEND_FAIL) {
                            log_e("Data send to " MACSTR " failed.", MAC2STR(send_cb->mac_addr));
                        } else {
                            log_v("Data send to " MACSTR " successful.", MAC2STR(send_cb->mac_addr));
                        }
                        break;
                    }
                case ESPNOW_RECV_CB:
                    {
                        espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                        espnow_data_t *data             = (espnow_data_t *)recv_cb->data;
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

                                    log_d("Received node state from " MACSTR ": type=%d, color=%d, currentState=%d, battery=%dmV (%d%%)", MAC2STR(recv_cb->mac_addr), node_info->node_type, node_info->color, node_info->current_state, node_info->battery_voltage, node_info->battery_percent);

                                    boolean notSeenBefore = false;
                                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                                        notSeenBefore = true;
                                        /* If MAC address does not exist in peer list, add it to peer list. */
                                        esp_now_peer_info_t *peer = malloc_peer_info(recv_cb->mac_addr);
                                        log_v("Adding peer to list (" MACSTR ").", MAC2STR(peer->peer_addr));
                                        ESP_ERROR_CHECK(esp_now_add_peer(peer));
                                        free(peer);
                                    }

                                    peer_data_t *peer_data;
                                    ESP_ERROR_CHECK(get_or_create_peer_info(recv_cb->mac_addr, &peer_data));

                                    peer_data->last_seen     = time;
                                    peer_data->valid_version = (node_info->version == VERSION_CODE);
                                    if (peer_data->valid_version) {
                                        get_current_mode()->onReceiveState(peer_data, node_info);
                                        memcpy(&peer_data->node_info, node_info, sizeof(payload_node_info_t));
                                    } else {
                                        log_d("Received message from peer with invalid version (%d)", node_info->version);
                                    }

                                    if (notSeenBefore) {
                                        /* Ping when we first see them */
                                        send_ping(recv_cb->mac_addr);
                                    }

                                    if (node_info->node_type == NODE_TYPE_CONTROLLER) {
                                        time_of_last_keep_alive_communication = time; // When a controller is present -> prevent sleeping
                                    }

                                    bluetooth_notify_peer_list_changed();
                                }
                                break;

                            case ESP_DATA_TYPE_PING_PONG:
                                {
                                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                                        log_v("Ignoring ping from unknown peer %2x:%2x:%2x:%2x:%2x:%2x", recv_cb->mac_addr[0], recv_cb->mac_addr[1], recv_cb->mac_addr[2], recv_cb->mac_addr[3], recv_cb->mac_addr[4], recv_cb->mac_addr[5]);
                                        break;
                                    }

                                    ping_pong_stage_t stage = data->payload.ping_pong.stage;
                                    peer_data_t *peer_data;
                                    if (get_peer_info(recv_cb->mac_addr, &peer_data) == ESP_OK && peer_data->valid_version) {
                                        unsigned long time_us = micros();
                                        if (stage > PING_PONG_STAGE_PING) {
                                            peer_data->latency_us = min(65535UL, time_us - peer_data->last_sent_ping_us);
                                        }
                                        if (stage < PING_PONG_STAGE_DATA2) {
                                            espnow_data_t pong = {
                                                .type    = ESP_DATA_TYPE_PING_PONG,
                                                .payload = {
                                                    .ping_pong = {
                                                        .stage      = (ping_pong_stage_t)(stage + 1),
                                                        .latency_us = peer_data->latency_us,
                                                        .rssi       = peer_data->rssi,
                                                    },
                                                },
                                            };

                                            esp_err_t ret = esp_now_send(recv_cb->mac_addr, (const uint8_t *)&pong, sizeof(pong));
                                            if (ret != ESP_OK) {
                                                log_e("Send error: %s", esp_err_to_name(ret));
                                            }
                                        }

                                        // log_d("%s (stage=%d, latency=%dus, rssi=%ddBm)", (stage % 2) == 0 ? "Ping" : "Pong", stage, data->payload.ping_pong.latency_us, data->payload.ping_pong.rssi);
                                        peer_data->last_sent_ping_us = time_us;

                                        if (stage > PING_PONG_STAGE_PONG) {
                                            log_v("Connection info: %2x:%2x:%2x:%2x:%2x:%2x: latency: %dus, rssi=%d", recv_cb->mac_addr[0], recv_cb->mac_addr[1], recv_cb->mac_addr[2], recv_cb->mac_addr[3], recv_cb->mac_addr[4], recv_cb->mac_addr[5], peer_data->latency_us, peer_data->rssi);
                                        }
                                    }
                                }
                                break;
                            case ESP_DATA_TYPE_COMMAND:
                                executeCommand(NULL, &data->payload.command, recv_cb->data_len - sizeof(espnow_data_type_t));
                                break;
                            default:
                                log_e("Unknown data packet received (type=%d)", data->type);
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
            EVERY_N_SECONDS(5) { cleanup_peer_list(); }

            EVERY_N_SECONDS(ACCOUNCEMENT_INTERVAL_SECONDS) { send_state_update(); }

            if (!has_external_power) {
                /* Check for ">" in both of these time delta checks, otherwise an integer underflow will occur */
                if ((time > time_of_last_keep_alive_communication) && (time - time_of_last_keep_alive_communication > (SHUTDOWN_TIME_NO_BUZZING_SECONDS * 1000))) {
                    log_i("Nobody pushing any buttons. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                } else if ((time > time_of_last_seen_peer) && (time - time_of_last_seen_peer > (SHUTDOWN_TIME_NO_COMMS_SECONDS * 1000))) {
                    log_i("No other buzzer near me. Shutting down...");
                    FastLED.setBrightness(10);
                    shutdown(false, true);
                }
            }

            EVERY_N_MILLIS_I(peerPing, pingInterval) {
                if (pingInterval != 0) {
                    static uint8_t peerToPing = 0;
                    bool head                 = true;
                    esp_now_peer_info_t peer;
                    peer_data_t *peer_data;
                    uint8_t i = 0;
                    while (esp_now_fetch_peer(head, &peer) == ESP_OK) {
                        head = false;
                        if (i++ < peerToPing) {
                            continue;
                        }

                        if (get_peer_info(peer.peer_addr, &peer_data) == ESP_OK && peer_data->valid_version) {
                            send_ping(peer.peer_addr);
                            break;
                        }
                    }
                    peerToPing++;
                    if (esp_now_fetch_peer(head, &peer) != ESP_OK) {
                        peerToPing = 0;
                    }
                }
            }
            peerPing.setPeriod(pingInterval);
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t espnow_init(void) {
    esp_base_mac_addr_get(my_mac_addr);

    s_comm_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_comm_queue == NULL) {
        log_e("Create mutex fail");
        return ESP_FAIL;
    }

    /* init peer data */
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        memset(peer_data_table[i].mac_addr, 0xFF, ESP_NOW_ETH_ALEN);
    }

    if (!has_external_power) {
        /* If we're not a controller, the first peer is ourself */
        memcpy(peer_data_table[0].mac_addr, my_mac_addr, ESP_NOW_ETH_ALEN);
        peer_data_table[0].latency_us    = 0;
        peer_data_table[0].rssi          = 0;
        peer_data_table[0].valid_version = true;
        update_my_info();
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc_peer_info(s_broadcast_mac);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    xTaskCreate(&comm_task, "comm_task", 2400, NULL, TASK_PRIO_COMM, NULL);

    return ESP_OK;
}

typedef struct
{
    unsigned frame_ctrl  : 16;   // 2 bytes / 16 bit fields
    unsigned duration_id : 16;   // 2 bytes / 16 bit fields
    uint8_t addr1[6];            // receiver address
    uint8_t addr2[6];            // sender address
    uint8_t addr3[6];            // filtering address
    unsigned sequence_ctrl : 16; // 2 bytes / 16 bit fields
} wifi_ieee80211_mac_hdr_t;      // 24 bytes

typedef struct
{
    wifi_ieee80211_mac_hdr_t hdr;
    unsigned category_code : 8; // 1 byte / 8 bit fields
    uint8_t oui[3];             // 3 bytes / 24 bit fields
    uint8_t payload[0];
} wifi_ieee80211_packet_t;

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) {
        /* Should never happen because we are filtering */
        return;
    }

    static const uint8_t ACTION_SUBTYPE  = 0xd0;
    static const uint8_t ESPRESSIF_OUI[] = { 0x18, 0xfe, 0x34 };
    static peer_data_t *peer_data;

    const wifi_promiscuous_pkt_t *ppkt  = (wifi_promiscuous_pkt_t *)buf;
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
void comm_setup(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif

    espnow_init();
}