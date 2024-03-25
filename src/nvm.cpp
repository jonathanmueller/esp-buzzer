#include "nvm.h"

#include "EEPROM.h"

nvm_data_t nvm_data;

void nvm_setup() {
    EEPROM.begin(sizeof(nvm_data_t));
    EEPROM.readBytes(0, &nvm_data, sizeof(nvm_data_t));
    if (nvm_data.MAGIC_BYTE != EEPROM_MAGIC_BYTE) {
        log_i("Initializing EEPROM");
        nvm_data = {
            .MAGIC_BYTE = EEPROM_MAGIC_BYTE,
            .color      = COLOR_ORANGE,
            .rgb        = { 255, 0, 0 }
        };
        nvm_save();
    }
}

void nvm_save() {
    EEPROM.writeBytes(0, &nvm_data, sizeof(nvm_data_t));
    if (!EEPROM.commit()) {
        log_e("Error writing to EEPROM");
    }
}