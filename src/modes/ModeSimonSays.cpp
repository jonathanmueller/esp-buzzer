#include "modes/ModeSimonSays.h"

ModeSimonSays::ModeSimonSays() : IMode(MODE_SIMON_SAYS) {}

void ModeSimonSays::setup() {}
void ModeSimonSays::update_my_info(payload_node_info_t *node_info) {}
void ModeSimonSays::onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {}

const CRGB simonSaysColors[] = {
    CRGB::Red,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Blue
};
uint8_t simonSaysColorIndex = 0;

void ModeSimonSays::loop() {
    EVERY_N_MILLIS(500) {
        simonSaysColorIndex++;
        if (simonSaysColorIndex >= sizeof(simonSaysColors) / sizeof(CRGB)) {
            simonSaysColorIndex = 0;
        }
    }
}

void ModeSimonSays::display() {
    fill_solid(leds, NUM_LEDS, simonSaysColors[simonSaysColorIndex].scale8(64));
}

ModeSimonSays modeSimonSays;