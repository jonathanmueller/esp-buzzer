#include "modes/ModeSimonSays.h"
#include <random>

ModeSimonSays::ModeSimonSays() : IMode(MODE_SIMON_SAYS) {}

void ModeSimonSays::setup() {
    IMode::setup();
    /* fill broadcast mac to game_participants */
    memset(this->game_participants, 0xFF, sizeof(this->game_participants));
    memcpy(this->game_participants[0], my_mac_addr, ESP_NOW_ETH_ALEN);

    /* fill game_participants with peers that are in simon says mode and idle state */
    peer_data_t *peer_data;
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(peer_data_table[i].mac_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            continue;
        }

        if (peer_data_table[i].node_info.current_mode == MODE_SIMON_SAYS &&
            peer_data_table[i].node_info.current_mode_state.node_state_simon_says == MODE_SIMON_SAYS_STATE_IDLE) {
            this->insert_peer(peer_data_table[i].mac_addr);
        }
    }
}
void ModeSimonSays::update_mode_specific_state(mode_specific_state_t *mode_state) {
}

int cmp_mac(mac_addr_t a, mac_addr_t b) {
    return memcmp(a, b, ESP_NOW_ETH_ALEN);
}

void ModeSimonSays::insert_peer(mac_addr_t mac_addr) {
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        int cmp = memcmp(mac_addr, this->game_participants[i], ESP_NOW_ETH_ALEN);
        if (cmp == 0) {
            /* Peer already in list, do nothing */
            return;
        } else if (cmp < 0) {
            /* Peer is before this entry, insert here */
            for (uint8_t j = PEER_DATA_TABLE_ENTRIES - 1; j > i; j--) {
                memcpy(this->game_participants[j], this->game_participants[j - 1], ESP_NOW_ETH_ALEN);
            }
            memcpy(this->game_participants[i], mac_addr, ESP_NOW_ETH_ALEN);
            log_d("Added new peer. Peer list:");
            for (uint8_t j = 0; j < PEER_DATA_TABLE_ENTRIES; j++) {
                log_d("%02X:%02X:%02X:%02X:%02X:%02X", this->game_participants[j][0], this->game_participants[j][1], this->game_participants[j][2], this->game_participants[j][3], this->game_participants[j][4], this->game_participants[j][5]);
            }
            return;
        }
    }
    log_e("Could not insert new peer.");
}
void ModeSimonSays::remove_peer(mac_addr_t mac_addr) {
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        int cmp = memcmp(mac_addr, this->game_participants[i], ESP_NOW_ETH_ALEN);
        if (cmp == 0) {
            /* Peer is in list, remove it */
            for (uint8_t j = i; j < PEER_DATA_TABLE_ENTRIES - 1; j++) {
                memcpy(this->game_participants[j], this->game_participants[j + 1], ESP_NOW_ETH_ALEN);
            }
            memcpy(this->game_participants[PEER_DATA_TABLE_ENTRIES - 1], s_broadcast_mac, ESP_NOW_ETH_ALEN);
            return;
        }
    }
}

const CRGB simonSaysColors[] = {
    CRGB::Red,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Blue
};
uint8_t simonSaysColorIndex = 0;
#define SIMON_SAYS_NUM_COLORS (sizeof(simonSaysColors) / sizeof(simonSaysColors[0]))
void ModeSimonSays::loop() {
    bool buzzerButtonPressed = false;
    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        buzzerButtonPressed          = !this->lastPushedBuzzerButton;
        this->lastPushedBuzzerButton = true;
    } else {
        this->lastPushedBuzzerButton = false;
    }

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            EVERY_N_MILLIS(500) {
                simonSaysColorIndex++;
                if (simonSaysColorIndex >= SIMON_SAYS_NUM_COLORS) {
                    simonSaysColorIndex = 0;
                }
            }

            break;
    }
}

void ModeSimonSays::onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {
    if (received_state->current_mode != MODE_SIMON_SAYS) {
        if (this->getState<node_state_simon_says_t>() == MODE_SIMON_SAYS_STATE_IDLE) {
            /* Only remove peers if we're in idle state, to not mess up game state */
            this->remove_peer(previous_state->mac_addr);
        }
        return;
    }

    auto otherState = received_state->current_mode_state.node_state_simon_says;

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            if (otherState == MODE_SIMON_SAYS_STATE_IDLE) {
                this->insert_peer(previous_state->mac_addr);
            }

            break;
    }
}

uint8_t ModeSimonSays::count_participants() {
    uint8_t num_participants = 0;
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            break;
        }
        num_participants++;
    }
    return num_participants;
}

void ModeSimonSays::display() {
    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            /* show game participants */
            fill_solid(leds, NUM_LEDS, 0);
            uint8_t num_participants     = this->count_participants();
            uint8_t LEDS_PER_PARTICIPANT = NUM_LEDS / num_participants;
            for (uint8_t i = 0; i < num_participants; i++) {
                if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
                    break;
                } else if (memcmp(this->game_participants[i], my_mac_addr, ESP_NOW_ETH_ALEN) == 0) {
                    fill_solid(&leds[i * LEDS_PER_PARTICIPANT], LEDS_PER_PARTICIPANT, baseColor);
                    continue;
                }
                peer_data_t *peer_data;
                esp_err_t ret = get_peer_info(this->game_participants[i], &peer_data);
                if (ret == ESP_OK) {
                    fill_solid(&leds[i * LEDS_PER_PARTICIPANT], LEDS_PER_PARTICIPANT, get_effective_color(peer_data->node_info.color, peer_data->node_info.rgb));
                }
            }

            if (num_participants > 0 && num_participants * LEDS_PER_PARTICIPANT < NUM_LEDS) {
                fill_solid(&leds[num_participants * LEDS_PER_PARTICIPANT], NUM_LEDS - num_participants * LEDS_PER_PARTICIPANT, leds[(num_participants - 1) * LEDS_PER_PARTICIPANT]);
            }
            break;
    }
}

ModeSimonSays *modeSimonSays = new ModeSimonSays();