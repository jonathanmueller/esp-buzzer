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
    bool lastPushedBuzzerButton = false; /* Whether or not the buzzer button was pushed last loop iteration */
    void insert_peer(mac_addr_t mac_addr);
    void remove_peer(mac_addr_t mac_addr);
    uint8_t count_participants();

    /* Sorted list of game participants. Should be the same on all peers */
    mac_addr_t game_participants[PEER_DATA_TABLE_ENTRIES];
};

extern ModeSimonSays *modeSimonSays;