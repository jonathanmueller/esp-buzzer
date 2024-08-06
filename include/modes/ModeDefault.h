#pragma once

#include "IMode.h"

class ModeDefault : public IMode {
  public:
    unsigned long buzzer_active_until;
    unsigned long buzzer_disabled_until;

    ModeDefault();
    ~ModeDefault() {};

    void setup();
    void update_my_info(payload_node_info_t *node_info);
    void onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state);
    void loop();
    void display();
    void setActive(bool active);
    void buzz();
    bool cleanup_peer_data(peer_data_t *peer_data);
};

extern ModeDefault modeDefault;