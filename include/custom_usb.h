#pragma once

#include "USB.h"

#ifndef CONFIG_TINYUSB_ENABLED
#define usb_setup()
#else

#include "USBVendor.h"
#include "USBHIDKeyboard.h"

extern USBVendor Vendor;
extern USBHIDKeyboard Keyboard;

void usb_setup();
#endif