#include "nvm.h"

#include "EEPROM.h"
#include "esp_rom_crc.h"

nvm_data_t nvm_data;

void nvm_setup() {
    EEPROM.begin(sizeof(nvm_data_t));
    EEPROM.readBytes(0, &nvm_data, sizeof(nvm_data_t));
    if (nvm_data.MAGIC_BYTE != EEPROM_MAGIC_BYTE) {
        log_i("Initializing EEPROM");
        nvm_data = {
            .MAGIC_BYTE  = EEPROM_MAGIC_BYTE,
            .color       = COLOR_ORANGE,
            .rgb         = { 255, 0, 0 },
            .game_config = {
                .buzzer_active_time              = BUZZER_ACTIVE_TIME,
                .deactivation_time_after_buzzing = BUZZER_DISABLED_TIME,
                .buzz_effect                     = EFFECT_FLASH_WHITE,
                .can_buzz_while_other_is_active  = false,
                .must_release_before_pressing    = true,
            },
            .key_config = { .modifiers = 0, .scan_code = 0 }
        };

        nvm_save();
    }
}

void nvm_save() {
    // Calculate CRC for game config
    nvm_data.game_config.crc = esp_rom_crc16_be(0, (const uint8_t *)&nvm_data.game_config, (const uint8_t *)&nvm_data.game_config.crc - (const uint8_t *)&nvm_data.game_config);

    EEPROM.writeBytes(0, &nvm_data, sizeof(nvm_data_t));
    if (!EEPROM.commit()) {
        log_e("Error writing to EEPROM");
    }
}