#include "modes/IMode.h"
#include "nvm.h"

static IMode *modes[node_mode_t::NUM_MODES] = { 0 };
IMode *get_mode(node_mode_t modeIdx) {
    IMode *mode = modes[modeIdx];
    if (mode == nullptr) {
        log_e("Mode %d not initialized", mode);
    }
    return mode;
}

IMode *get_current_mode() {
    return get_mode(nvm_data.mode);
}

void set_mode(node_mode_t mode) {
    if (nvm_data.mode != mode && mode < node_mode_t::NUM_MODES) {
        nvm_data.mode     = mode;
        last_state_change = millis();

        get_current_mode()->setup();
        nvm_save();
    }
}

/* class IMode */
IMode::IMode(node_mode_t mode) { modes[mode] = this; }

// node_mode_state_t IMode::getState() { return this->state; }

void IMode::_setState(node_mode_state_t state) {
    if (state.raw != this->state.raw) {
        this->last_state_change = millis();
    }

    this->state = state;
}

unsigned long IMode::getTimeSinceLastStateChange() { return millis() - this->last_state_change; }

void IMode::setup() { this->state = { .raw = 0 }; }
/* end class IMode*/