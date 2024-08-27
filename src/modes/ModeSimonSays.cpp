#include "modes/ModeSimonSays.h"
#include "esp_crc.h"
#include "esp_random.h"

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

    this->participants_updated();
    this->my_seed                  = (uint16_t)(esp_random() % 0xFFFF);
    this->show_error_until         = 0;
    this->game_crc                 = this->calc_game_config_crc();
    this->current_sequence_length  = 0;
    this->current_sequence_correct = 0;
}

void ModeSimonSays::participants_updated() {
    uint8_t num_participants     = 0;
    uint8_t my_participant_index = 0;

    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            break;
        }

        num_participants++;
        if (memcmp(this->game_participants[i], my_mac_addr, ESP_NOW_ETH_ALEN) == 0) {
            my_participant_index = i;
        }
    }

    this->num_participants     = num_participants;
    this->my_participant_index = my_participant_index;
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
            log_d("Added new peer.");

            log_v("Peer list:");
            for (uint8_t j = 0; j < PEER_DATA_TABLE_ENTRIES; j++) {
                if (memcmp(this->game_participants[j], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
                    break;
                }
                log_v("%02X:%02X:%02X:%02X:%02X:%02X", this->game_participants[j][0], this->game_participants[j][1], this->game_participants[j][2], this->game_participants[j][3], this->game_participants[j][4], this->game_participants[j][5]);
            }
            this->participants_updated();
            return;
        }
    }
    log_e("Could not insert new peer.");
    for (uint8_t j = 0; j < PEER_DATA_TABLE_ENTRIES; j++) {
        if (memcmp(this->game_participants[j], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            break;
        }
        log_e("%02X:%02X:%02X:%02X:%02X:%02X", this->game_participants[j][0], this->game_participants[j][1], this->game_participants[j][2], this->game_participants[j][3], this->game_participants[j][4], this->game_participants[j][5]);
    }
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

            this->participants_updated();
            return;
        }
    }
}

void ModeSimonSays::update_mode_specific_state(mode_specific_state_t *mode_state) {
    mode_state->mode_simon_says = {
        .seed                     = this->my_seed,
        .game_config_crc          = this->game_crc,
        .current_sequence_length  = this->current_sequence_length,
        .current_sequence_correct = this->current_sequence_correct
    };

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE:
            mode_state->mode_simon_says.time_since_state_change = millis() - this->time_when_state_started;
            break;
    }
}

uint8_t ModeSimonSays::get_sequence(uint16_t segment) {
    srand(this->game_crc);

    uint8_t ret = 0;
    for (uint16_t i = 0; i <= segment; i++) {
        ret = (rand() % this->num_participants);
    }
    return ret;
}
void ModeSimonSays::reset_game() {
    log_e("Reset game");
    this->setState(MODE_SIMON_SAYS_STATE_IDLE);
    this->time_when_state_started = millis();
    this->setup();
    send_state_update();
}

void ModeSimonSays::remove_unknown_peers() {
    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(this->game_participants[PEER_DATA_TABLE_ENTRIES - 1 - i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            continue;
        }
        peer_data_t *peer_data;
        esp_err_t ret = get_peer_info(this->game_participants[PEER_DATA_TABLE_ENTRIES - 1 - i], &peer_data);
        if (ret != ESP_OK) {
            log_e("Removing unknown peer...");
            this->remove_peer(this->game_participants[PEER_DATA_TABLE_ENTRIES - 1 - i]);
        }
    }
}

void ModeSimonSays::loop() {
    bool buzzerButtonPressed = false;
    static CEveryNMillis debounceBuzzerButton(50);
    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        buzzerButtonPressed          = !this->lastPushedBuzzerButton;
        this->lastPushedBuzzerButton = true;
        debounceBuzzerButton.reset();
    } else {
        if (debounceBuzzerButton) {
            this->lastPushedBuzzerButton = false;
        }
    }

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            if (buzzerButtonPressed) {
                if (this->is_valid_game()) {
                    log_d("Starting new game.");

                    this->setState(MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE);
                    this->time_when_state_started  = millis();
                    this->current_sequence_length  = 1;
                    this->current_sequence_correct = 0;
                    send_state_update();
                } else {
                    this->show_error_until = millis() + 1000;
                }
            }

            this->remove_unknown_peers();

            break;
        case MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE:
            {
                if (!this->is_valid_game()) {
                    this->reset_game();
                    this->show_error_until = millis() + 1000;
                    break;
                }

                unsigned long time_since_state_change = millis() - this->time_when_state_started;

                if (time_since_state_change <= SIMON_SAYS_INITIAL_DELAY) {
                    this->should_flash = false;
                } else {
                    time_since_state_change -= SIMON_SAYS_INITIAL_DELAY;

                    uint16_t segment              = time_since_state_change / (SIMON_SAYS_LIGHTUP_DURATION + SIMON_SAYS_PAUSE_DURATION);
                    unsigned long time_in_segment = time_since_state_change - (segment * (SIMON_SAYS_LIGHTUP_DURATION + SIMON_SAYS_PAUSE_DURATION));

                    if (segment >= this->current_sequence_length) {
                        this->setState(MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER);
                        this->time_when_state_started = millis();
                        send_state_update();
                    } else {
                        if (time_in_segment <= SIMON_SAYS_LIGHTUP_DURATION && this->get_sequence(segment) == this->my_participant_index) {
                            this->should_flash = true;
                        } else {
                            this->should_flash = false;
                        }

                        if (buzzerButtonPressed && segment == this->current_sequence_length - 1) {
                            log_e("fast press after showing sequence.");

                            bool is_correct_press = this->get_sequence(0) == this->my_participant_index;
                            if (is_correct_press) {
                                this->current_sequence_correct = 1;
                            } else {
                                this->current_sequence_correct = 0;
                            }
                            this->setState(MODE_SIMON_SAYS_STATE_SHOWING_RESULT);
                            this->time_when_state_started = millis();
                            send_state_update();
                            break;
                        }

                        EVERY_N_MILLIS(500 + (esp_random() % 100)) {
                            send_state_update();
                        }
                    }
                }
            }
            break;
        case MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER:
            if (!this->is_valid_game()) {
                this->reset_game();
                this->show_error_until = millis() + 1000;
                break;
            }

            this->should_flash = false;
            if (buzzerButtonPressed) {
                this->should_flash    = true; /* So that the state SHOWING_RESULT will flash the current buzzer */
                bool is_correct_press = this->get_sequence(this->current_sequence_correct) == this->my_participant_index;
                if (is_correct_press) {
                    this->current_sequence_correct++;
                } else {
                    this->current_sequence_correct = 0;
                }
                this->setState(MODE_SIMON_SAYS_STATE_SHOWING_RESULT);
                this->time_when_state_started = millis();
                send_state_update();
            }
            break;
        case MODE_SIMON_SAYS_STATE_SHOWING_RESULT:
            {
                unsigned long time_since_state_change = millis() - this->time_when_state_started;

                bool correct_press_and_sequence_not_done = this->current_sequence_correct > 0 && this->current_sequence_correct < this->current_sequence_length;

                if (time_since_state_change > 50 && buzzerButtonPressed && correct_press_and_sequence_not_done) {
                    /* Allow pressing the next button while showing result (after 200ms) */
                    this->should_flash    = true; /* So that the state SHOWING_RESULT will flash the current buzzer */
                    bool is_correct_press = this->get_sequence(this->current_sequence_correct) == this->my_participant_index;
                    if (is_correct_press) {
                        this->current_sequence_correct++;
                    } else {
                        this->current_sequence_correct = 0;
                    }
                    this->setState(MODE_SIMON_SAYS_STATE_SHOWING_RESULT);
                    this->time_when_state_started = millis();
                    send_state_update();
                    break;
                }

                if (time_since_state_change > SIMON_SAYS_LIGHTUP_DURATION + SIMON_SAYS_PAUSE_DURATION) {
                    if (correct_press_and_sequence_not_done) {
                        /* This button was correct, but we are not done with the sequence. Wait for next button */
                        this->setState(MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER);
                        this->time_when_state_started = millis();
                        send_state_update();
                    } else {
                        if (this->current_sequence_correct > 0) {
                            /* This button was correct, and we are done with the sequence. Next round */
                            this->current_sequence_length++;
                            this->current_sequence_correct = 0;
                            this->setState(MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE);
                            this->time_when_state_started = millis();
                            send_state_update();
                        } else {
                            if (time_since_state_change > SIMON_SAYS_LIGHTUP_DURATION + SIMON_SAYS_PAUSE_DURATION + SIMON_SAYS_RESULT_DURATION) {

                                /* This button was incorrect. Reset Game*/
                                this->reset_game();
                            }
                        }
                    }
                }
            }
            break;
    }
}

char const *MODE_NAMES[] = {
    [MODE_SIMON_SAYS_STATE_IDLE]               = "IDLE",
    [MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE]   = "PLAYING_SEQUENCE",
    [MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER] = "WAITING_FOR_PLAYER",
    [MODE_SIMON_SAYS_STATE_SHOWING_RESULT]     = "SHOWING_RESULT",
};

void ModeSimonSays::onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {
    if (received_state->current_mode != MODE_SIMON_SAYS) {
        if (this->getState<node_state_simon_says_t>() == MODE_SIMON_SAYS_STATE_IDLE) {
            /* Only remove peers if we're in idle state, to not mess up game state */
            this->remove_peer(previous_state->mac_addr);
        }
        return;
    }

    auto otherPreviousState = previous_state->node_info.current_mode_state.node_state_simon_says;
    auto otherState         = received_state->current_mode_state.node_state_simon_says;

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            if (otherState == MODE_SIMON_SAYS_STATE_IDLE) {
                this->insert_peer(previous_state->mac_addr);
            } else if (otherState == MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE) {
                log_d("Other peer started the game");

                /* Another peer started the game, we start as well */
                this->setState(MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE);
                this->current_sequence_length = received_state->mode_specific_state.mode_simon_says.current_sequence_length;
                this->time_when_state_started = millis() - received_state->mode_specific_state.mode_simon_says.time_since_state_change;
                schedule_state_update();
            }
        case MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER:
        case MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE:
            if (otherState == MODE_SIMON_SAYS_STATE_SHOWING_RESULT) {
                /* Another button was pressed. Go into show result */
                log_d("Other buzzer pressed. Take their state");
                this->setState(MODE_SIMON_SAYS_STATE_SHOWING_RESULT);
                this->current_sequence_length  = received_state->mode_specific_state.mode_simon_says.current_sequence_length;
                this->current_sequence_correct = received_state->mode_specific_state.mode_simon_says.current_sequence_correct;
                this->time_when_state_started  = millis() - received_state->mode_specific_state.mode_simon_says.time_since_state_change;
                schedule_state_update();
            }
            break;
        case MODE_SIMON_SAYS_STATE_SHOWING_RESULT:
            switch (otherState) {
                case MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER:
                    // log_d("Another buzzer went back to waiting");
                    // /* Another buzzer went back to the next waiting for player state. We also go there and update the correctness counter */
                    // this->setState(MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER);
                    // this->current_sequence_length  = received_state->mode_specific_state.mode_simon_says.current_sequence_length;
                    // this->current_sequence_correct = received_state->mode_specific_state.mode_simon_says.current_sequence_correct;
                    // this->time_when_state_started  = millis() - received_state->mode_specific_state.mode_simon_says.time_since_state_change;
                    // schedule_state_update();
                    break;
                case MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE:
                    /* Another is back into playback mode, we follow only if sequence length increased */
                    if (received_state->mode_specific_state.mode_simon_says.current_sequence_length > this->current_sequence_length) {
                        this->setState(MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE);
                        this->current_sequence_length  = received_state->mode_specific_state.mode_simon_says.current_sequence_length;
                        this->current_sequence_correct = received_state->mode_specific_state.mode_simon_says.current_sequence_correct;
                        this->time_when_state_started  = millis() - received_state->mode_specific_state.mode_simon_says.time_since_state_change;
                        schedule_state_update();
                    }
                    break;
                case MODE_SIMON_SAYS_STATE_SHOWING_RESULT:
                    /* Another buzzer went back to the next waiting for player state. We also go there and update the correctness counter */
                    this->setState(MODE_SIMON_SAYS_STATE_SHOWING_RESULT);
                    bool some_value_has_changed = false;
                    if (this->current_sequence_length != received_state->mode_specific_state.mode_simon_says.current_sequence_length) {
                        some_value_has_changed        = true;
                        this->current_sequence_length = received_state->mode_specific_state.mode_simon_says.current_sequence_length;
                    }
                    if (this->current_sequence_correct != received_state->mode_specific_state.mode_simon_says.current_sequence_correct) {
                        some_value_has_changed         = true;
                        this->current_sequence_correct = received_state->mode_specific_state.mode_simon_says.current_sequence_correct;
                    }
                    if (some_value_has_changed) {
                        log_d("Other buzzer pressed. Take their state");
                        this->should_flash            = false; /* We were not pressed, so we want to disable our light */
                        this->time_when_state_started = millis() - received_state->mode_specific_state.mode_simon_says.time_since_state_change;
                        schedule_state_update();
                    }
                    break;
            }

            break;
    }

    log_v("RX: state=%s, seq: %d, correct: %d", MODE_NAMES[otherState], received_state->mode_specific_state.mode_simon_says.current_sequence_length, received_state->mode_specific_state.mode_simon_says.current_sequence_correct);
    log_v("MY: state=%s, seq: %d, correct: %d", MODE_NAMES[this->getState<node_state_simon_says_t>()], this->current_sequence_length, this->current_sequence_correct);
    log_v("");

    uint16_t new_crc = this->calc_game_config_crc();
    if (this->game_crc != new_crc) {
        this->game_crc = new_crc;
        schedule_state_update();
    }
}

uint16_t ModeSimonSays::calc_game_config_crc() {
    uint16_t crc = 0;

    /* CRC Input: Number of participants */
    crc = esp_rom_crc16_be(crc, (const uint8_t *)&this->num_participants, sizeof(uint8_t));

    for (uint8_t i = 0; i < this->num_participants; i++) {
        if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            break;
        }

        mac_addr_t *mac = &this->game_participants[i];

        /* CRC Input: Participant's mac address */
        crc = esp_rom_crc16_be(crc, (const uint8_t *)mac, sizeof(mac_addr_t));

        peer_data_t *peer_data;
        esp_err_t ret = get_peer_info(this->game_participants[i], &peer_data);
        if (ret == ESP_OK) {
            /* CRC Input: Participant's seed */
            crc = esp_rom_crc16_be(crc, (const uint8_t *)&peer_data->node_info.mode_specific_state.mode_simon_says.seed, sizeof(uint16_t));
        }
    }

    log_v("Game CRC: %04X", crc);
    return crc;
}

bool ModeSimonSays::is_valid_game() {
    if (this->num_participants < 2) {
        return false;
    }

    for (uint8_t i = 0; i < PEER_DATA_TABLE_ENTRIES; i++) {
        if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
            break;
        }
        peer_data_t *peer_data;
        esp_err_t ret = get_peer_info(this->game_participants[i], &peer_data);
        if (ret == ESP_OK) {
            if (peer_data->node_info.mode_specific_state.mode_simon_says.game_config_crc != this->game_crc) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

void ModeSimonSays::display() {
    unsigned long synced_time = (millis() - this->time_when_state_started);

    switch (this->getState<node_state_simon_says_t>()) {
        case MODE_SIMON_SAYS_STATE_IDLE:
            if (this->show_error_until > millis()) {
                CRGB errorColor = CRGB::Red;
                fill_solid(leds, NUM_LEDS, errorColor.scale8(127 + sin8((millis()) % 255) / 2));

            } else {
                /* show game participants */
                fill_solid(leds, NUM_LEDS, 0);
                uint8_t LEDS_PER_PARTICIPANT = NUM_LEDS / this->num_participants;
                for (uint8_t i = 0; i < this->num_participants; i++) {
                    if (memcmp(this->game_participants[i], s_broadcast_mac, ESP_NOW_ETH_ALEN) == 0) {
                        break;
                    } else if (memcmp(this->game_participants[i], my_mac_addr, ESP_NOW_ETH_ALEN) == 0) {
                        fill_solid(&leds[i * LEDS_PER_PARTICIPANT], LEDS_PER_PARTICIPANT, baseColor.scale8(16 + sin8((millis() / 2) % 255) / 8));
                        continue;
                    }
                    peer_data_t *peer_data;
                    esp_err_t ret = get_peer_info(this->game_participants[i], &peer_data);
                    if (ret == ESP_OK) {
                        fill_solid(&leds[i * LEDS_PER_PARTICIPANT], LEDS_PER_PARTICIPANT, get_effective_color(peer_data->node_info.color, peer_data->node_info.rgb).scale8(8));
                    }
                }

                if (this->num_participants > 0 && this->num_participants * LEDS_PER_PARTICIPANT < NUM_LEDS) {
                    fill_solid(&leds[this->num_participants * LEDS_PER_PARTICIPANT], NUM_LEDS - this->num_participants * LEDS_PER_PARTICIPANT, leds[(this->num_participants - 1) * LEDS_PER_PARTICIPANT]);
                }
            }
            break;
        case MODE_SIMON_SAYS_STATE_PLAYING_SEQUENCE:
            fill_solid(leds, NUM_LEDS, this->should_flash ? baseColor : CRGB::Black);
            break;
        case MODE_SIMON_SAYS_STATE_WAITING_FOR_PLAYER:
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            // {
            //     uint8_t brightness = 1 + sin8((synced_time) / 8) / 32;
            //     CRGB color(brightness, brightness, brightness);
            //     if (synced_time < 2000) {
            //         color.nscale8_video(synced_time * (255.0 / 2000.0));
            //     }
            //     fill_solid(leds, NUM_LEDS, color);
            // }
            break;
        case MODE_SIMON_SAYS_STATE_SHOWING_RESULT:
            {
                unsigned long time_since_state_change = millis() - this->time_when_state_started;

                if (time_since_state_change <= 50) { /* To allow us to see consecutive presses */
                    fill_solid(leds, NUM_LEDS, CRGB::Black);
                } else if (time_since_state_change <= SIMON_SAYS_LIGHTUP_DURATION) {
                    fill_solid(leds, NUM_LEDS, this->should_flash ? baseColor : CRGB::Black);
                } else if (time_since_state_change <= SIMON_SAYS_LIGHTUP_DURATION + SIMON_SAYS_PAUSE_DURATION) {
                    fill_solid(leds, NUM_LEDS, CRGB::Black);
                } else {
                    CRGB color = this->current_sequence_correct > 0 ? CRGB::Black : CRGB::Red;
                    fill_solid(leds, NUM_LEDS, color.scale8(127 + sin8((millis() / 2) % 255) / 2));
                }
            }

            break;
    }
}

ModeSimonSays *modeSimonSays = new ModeSimonSays();