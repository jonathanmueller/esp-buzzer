#include "mode.h"
#include "modes/IMode.h"
#include "nvm.h"

node_state_t current_state      = STATE_DEFAULT;
unsigned long last_state_change = 0;
void set_state(node_state_t state) {
    current_state     = state;
    last_state_change = millis();
}

void mode_setup() {
    if (nvm_data.mode >= node_mode_t::NUM_MODES) {
        nvm_data.mode = node_mode_t::MODE_DEFAULT;
    }

    set_mode(nvm_data.mode);
}