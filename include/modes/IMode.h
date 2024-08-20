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

    /**
     * Returns the time elapsed (in milliseconds) since the state last changed.
     *
     * @return The time elapsed (in milliseconds) since the state last changed.
     */
    unsigned long getTimeSinceLastStateChange();

    /**
     * This function is called to set up the mode.
     * It is called whenever the mode is started.
     */
    virtual void setup();

    /**
     * This function is called to update the mode_specific_state of the current state.
     * Implementations can update the object with mode-specific details.
     *
     * @param mode_state Pointer to the mode_specific_state_t object to be updated.
     */
    virtual void update_mode_specific_state(mode_specific_state_t *mode_state) {};

    /**
     * This function is called when a state update is received from a peer node.
     *
     * The function can be used to handle changes in the state of a peer node. The function is called for
     * every peer node that is connected to this node.
     *
     * @param previous_state: A pointer to the previous node_info that was stored in the peer data table.
     * @param received_state: A pointer to the newly received node_info.
     */
    virtual void onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {};

    virtual void loop() {};

    /**
     * This function is called to clean up the peer data. It is called for
     * every peer data in the peer data table. If the peer data was touched
     * this function should return true.
     *
     * @param peer_data A pointer to the peer data to be cleaned up.
     * @return true If the peer data was changed
     * @return false If the peer data was not changed
     */
    virtual bool cleanup_peer_data(peer_data_t *peer_data) { return false; };

    /**
     * This function is responsible for displaying the current state of the mode.
     *
     * It is called from the main loop and should update the LEDs.
     */
    virtual void display() = 0;
};

extern IMode *get_current_mode();
extern IMode *get_mode(node_mode_t mode);
void set_mode(node_mode_t mode);