
#pragma once

#include <BLEServer.h>

void bluetooth_init();
bool bluetooth_connected();
void bluetooth_notify_peer_list_changed();

void bluetooth_set_state(bool state);
void bluetooth_loop();