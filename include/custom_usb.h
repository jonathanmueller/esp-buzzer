#pragma once

#include "USB.h"

#ifndef CONFIG_TINYUSB_ENABLED
#define usb_setup()
#else
void usb_setup();
#endif