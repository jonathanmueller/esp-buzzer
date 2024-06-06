#pragma once

#include "Arduino.h"
#include "led.h"
#include "button.h"
#include "comm.h"

#define EEPROM_MAGIC_BYTE 0x44
typedef struct {
    char MAGIC_BYTE;
    color_t color;
    uint8_t rgb[3];
    game_config_t game_config;
    key_config_t key_config;
} nvm_data_t;

extern nvm_data_t nvm_data;

void nvm_setup();
void nvm_save();