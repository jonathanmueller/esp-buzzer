#pragma once

#include "IMode.h"

class ModeSimonSays : public IMode {
  public:
    ModeSimonSays();
    ~ModeSimonSays() {};

    void setup();
    void update_my_info(payload_node_info_t *node_info);
    void onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state);
    void loop();
    void display();
};

extern ModeSimonSays *modeSimonSays;