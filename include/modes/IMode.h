#pragma once
#include "mode.h"
#include "comm.h"

class IMode {
  protected:
    node_mode_state_t state         = { .raw = 0 };
    unsigned long last_state_change = 0;
    void _setState(node_mode_state_t state);

  public:
    IMode(node_mode_t mode);

    virtual ~IMode() {};

    template <typename T>
    void setState(T state) { this->_setState({ .raw = (uint8_t)state }); }

    template <typename T = node_mode_state_t>
    T getState() { return *reinterpret_cast<T *>(&this->state); }

    unsigned long getTimeSinceLastStateChange();

    virtual void setup();
    virtual void update_my_info(payload_node_info_t *node_info) {};
    virtual void onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {};
    virtual void loop() {};
    virtual bool cleanup_peer_data(peer_data_t *cleanup_peer_data) { return false; };
    virtual void display() = 0;
};

extern IMode *get_current_mode();
extern IMode *get_mode(node_mode_t mode);
void set_mode(node_mode_t mode);