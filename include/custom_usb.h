#pragma once

#include "USB.h"

#ifndef CONFIG_TINYUSB_ENABLED
#define usb_setup()
#define usb_loop()
#else
void usb_setup();
void usb_loop();
#endif