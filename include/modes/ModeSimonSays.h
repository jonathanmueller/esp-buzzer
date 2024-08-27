#pragma once

#include "IMode.h"

class ModeSimonSays : public IMode {
  public:
    ModeSimonSays();
    ~ModeSimonSays() {};

    void setup();
    void update_mode_specific_state(mode_specific_state_t *mode_state);
    void onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state);
    void loop();
    void display();

  private:
    bool lastPushedBuzzerButton           = false; /* Whether or not the buzzer button was pushed last loop iteration */
    uint16_t game_crc                     = 0;
    uint16_t game_seed                    = 0;
    uint16_t current_sequence_length      = 0;
    uint16_t current_sequence_correct     = 0;
    unsigned long show_error_until        = 0;
    unsigned long time_when_state_started = 0;
    uint16_t my_seed                      = 0;
    uint8_t my_participant_index          = 0;
    uint8_t num_participants              = 0;

    /* is true while this buzzer should be on in the sequence */
    bool should_flash = false;

    void insert_peer(mac_addr_t mac_addr);
    void remove_peer(mac_addr_t mac_addr);
    void remove_unknown_peers();
    bool is_valid_game();
    void reset_game();
    uint16_t calc_game_config_crc();
    uint8_t get_sequence(uint16_t segment);

    void participants_updated();

    /* Sorted list of game participants. Should be the same on all peers */
    mac_addr_t game_participants[PEER_DATA_TABLE_ENTRIES];
};

extern ModeSimonSays *modeSimonSays;