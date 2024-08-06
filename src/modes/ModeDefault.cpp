#include "modes/ModeDefault.h"
#include "nvm.h"
#include "led.h"
#include "custom_usb.h"

static CEveryNMillis buzzStateUpdate(ACCOUNCEMENT_INTERVAL_WHILE_ACTIVE);

bool lastPushedBuzzerButton = false; /* Whether or not the buzzer button was pushed last loop iteration */

unsigned long buzzer_active_until   = 0;
unsigned long buzzer_disabled_until = 0;

ModeDefault::ModeDefault() : IMode(MODE_DEFAULT) {
}

void ModeDefault::onReceiveState(peer_data_t *previous_state, payload_node_info_t *received_state) {
    node_state_default_t peer_previous_state = previous_state->node_info.current_mode_state.node_state_default;

    unsigned long time = millis();

    // Handle state data
    if (this->getState<node_state_default_t>() != MODE_DEFAULT_STATE_BUZZER_ACTIVE &&
        received_state->current_mode_state.node_state_default == MODE_DEFAULT_STATE_BUZZER_ACTIVE) {

#ifdef CONFIG_TINYUSB_ENABLED
        if (peer_previous_state != MODE_DEFAULT_STATE_BUZZER_ACTIVE) {
            if ((received_state->key_config.modifiers & (1 << 0)) != 0) Keyboard.press(KEY_LEFT_CTRL);
            if ((received_state->key_config.modifiers & (1 << 1)) != 0) Keyboard.press(KEY_LEFT_ALT);
            if ((received_state->key_config.modifiers & (1 << 2)) != 0) Keyboard.press(KEY_LEFT_SHIFT);
            if ((received_state->key_config.modifiers & (1 << 3)) != 0) Keyboard.press(KEY_LEFT_GUI);
            if ((received_state->key_config.modifiers & (1 << 4)) != 0) Keyboard.press(KEY_RIGHT_CTRL);
            if ((received_state->key_config.modifiers & (1 << 5)) != 0) Keyboard.press(KEY_RIGHT_ALT);
            if ((received_state->key_config.modifiers & (1 << 6)) != 0) Keyboard.press(KEY_RIGHT_SHIFT);
            if ((received_state->key_config.modifiers & (1 << 7)) != 0) Keyboard.press(KEY_RIGHT_GUI);

            Keyboard.pressRaw(received_state->key_config.scan_code);
            Keyboard.releaseAll();
        }
#else
        (void)peer_previous_state; /* Silence "unused variable" warning */
#endif

        if (received_state->buzzer_active_remaining_ms > 0 &&
            this->buzzer_disabled_until < time + received_state->buzzer_active_remaining_ms) {
            reset_shutdown_timer();
            // time_of_last_keep_alive_communication = time; // This is a notable event -> reset shutdown timer

            if (!nvm_data.game_config.can_buzz_while_other_is_active) {
                this->buzzer_disabled_until = time + received_state->buzzer_active_remaining_ms;
                this->setState(MODE_DEFAULT_STATE_DISABLED);
                log_d("Received buzz from other node. Disabling for %dms", received_state->buzzer_active_remaining_ms);
            }
        }
    }
}

void ModeDefault::setup() {
}

void ModeDefault::display() {
    switch (this->getState<node_state_default_t>()) {
        case MODE_DEFAULT_STATE_IDLE:
            fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(BRIGHTNESS_IDLE));
            break;
        case MODE_DEFAULT_STATE_DISABLED:
            fill_solid(leds, NUM_LEDS, baseColor.nscale8_video(BRIGHTNESS_DISABLED));
            break;
        case MODE_DEFAULT_STATE_BUZZER_ACTIVE:
            uint8_t scale;
            uint8_t angle = (millis() / 10) % 256;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                leds[i] = baseColor;
                scale   = sin8((int16_t)(((int16_t)(ACTIVE_EFFECT_SPEED * angle) - ACTIVE_EFFECT_NUM_WAVES * (abs((int16_t)i - (NUM_LEDS / 2))) * 255.0f / NUM_LEDS)));
                scale   = scale < 50 ? 0 : (scale - 50) * (255.0 / (255.0 - 50));
                scale   = scale < 1 ? 1 : scale;
                leds[i].nscale8_video(scale);
            }

            unsigned long time_since_state_change = this->getTimeSinceLastStateChange();

            if (
                (nvm_data.game_config.buzz_effect == EFFECT_FLASH_BASE_COLOR ||
                 nvm_data.game_config.buzz_effect == EFFECT_FLASH_WHITE) &&
                time_since_state_change < FLASH_EFFECT_DURATION + (FLASH_EFFECT_PRE_FLASH_COUNT * FLASH_EFFECT_PRE_FLASH_DURATION)) {
                CRGB flashColor = nvm_data.game_config.buzz_effect == EFFECT_FLASH_BASE_COLOR ? baseColor : CRGB::White;

                fract8 fract;
                if (time_since_state_change < (FLASH_EFFECT_PRE_FLASH_COUNT * FLASH_EFFECT_PRE_FLASH_DURATION)) {
                    fract = 255 - (uint8_t)(255.0f * (time_since_state_change) / FLASH_EFFECT_PRE_FLASH_DURATION);
                } else {
                    fract = 255 - (255.0f * (time_since_state_change - (FLASH_EFFECT_PRE_FLASH_COUNT * FLASH_EFFECT_PRE_FLASH_DURATION)) / FLASH_EFFECT_DURATION);
                }
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    leds[i] = leds[i].lerp8(flashColor, fract);
                }
            }
            break;
    }
}

bool ModeDefault::cleanup_peer_data(peer_data_t *peer_data) {
    /* If the peer must be disabled by now, update */
    if (peer_data->node_info.current_mode == MODE_DEFAULT &&
        peer_data->node_info.current_mode_state.node_state_default == MODE_DEFAULT_STATE_BUZZER_ACTIVE &&
        (peer_data->node_info.buzzer_active_remaining_ms + peer_data->last_seen) < millis()) {
        peer_data->node_info.current_mode_state.node_state_default = MODE_DEFAULT_STATE_IDLE;
        return true;
    }
    return false;
}

void ModeDefault::update_my_info(payload_node_info_t *node_info) {
    unsigned long time                    = millis();
    node_info->buzzer_active_remaining_ms = this->getState<node_state_default_t>() == MODE_DEFAULT_STATE_BUZZER_ACTIVE
                                                ? (time > this->buzzer_active_until ? 0 : (this->buzzer_active_until - time))
                                                : 0;
}

void ModeDefault::setActive(bool active) {
    if (active) {
        this->setState(MODE_DEFAULT_STATE_DISABLED);
        this->buzzer_disabled_until = -1UL;
    } else {
        this->setState(MODE_DEFAULT_STATE_IDLE);
        this->buzzer_disabled_until = 0;
    }
}

void ModeDefault::loop() {
    unsigned long time = millis();

    if (this->getState<node_state_default_t>() == MODE_DEFAULT_STATE_BUZZER_ACTIVE && buzzStateUpdate) {
        send_state_update();
    }

    static CEveryNMillis debounceBuzzerButton(50);

    if (this->getState<node_state_default_t>() == MODE_DEFAULT_STATE_BUZZER_ACTIVE && time > this->buzzer_active_until) {
        log_d("Time's up! On cooldown for a bit.");
        this->setState(MODE_DEFAULT_STATE_DISABLED);
        if (this->buzzer_disabled_until != -1UL) {
            this->buzzer_disabled_until = time + nvm_data.game_config.deactivation_time_after_buzzing;
        }
        send_state_update();
    }

    if (this->getState<node_state_default_t>() == MODE_DEFAULT_STATE_DISABLED && time > this->buzzer_disabled_until) {
        log_d("Re-enabling.");
        this->setState(MODE_DEFAULT_STATE_IDLE);
        send_state_update();
    }

    if (digitalRead(BUZZER_BUTTON_PIN) == LOW) {
        if (!lastPushedBuzzerButton || !nvm_data.game_config.must_release_before_pressing) {
            if (this->getState<node_state_default_t>() == MODE_DEFAULT_STATE_IDLE) {
                this->buzz();
            }
        }
        lastPushedBuzzerButton = true;
        debounceBuzzerButton.reset();
    } else {
        if (debounceBuzzerButton) {
            lastPushedBuzzerButton = false;
        }
    }
}

ModeDefault modeDefault;

void ModeDefault::buzz() {
    log_i("BUZZ! Sending state update");

    unsigned long time = millis();
    modeDefault.setState(MODE_DEFAULT_STATE_BUZZER_ACTIVE);
    this->buzzer_active_until = time + nvm_data.game_config.buzzer_active_time;
    send_state_update();
    buzzStateUpdate.reset();

    /* This is notable! Reset shutdown timer */
    reset_shutdown_timer();
}