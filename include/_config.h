#pragma once

// Pins
#define BAT_VOLTAGE_PIN                     D2
#define BOOT_BUTTON_PIN                     D9
#define BUZZER_BUTTON_PIN                   D0
#define BACK_BUTTON_PIN                     D1
#define LED_PIN                             D4

// Battery
#define BAT_VOLTAGE_EXTERNAL_POWER          6000
#define BAT_VOLTAGE_100_PCT                 4200
#define BAT_VOLTAGE_50_PCT                  3700
#define BAT_VOLTAGE_0_PCT                   3200

#define BAT_VOLTAGE_FILTER_SIZE             16

#define LOW_BATTERY_THRESHOLD               0.01

// Game
#define BUZZER_ACTIVE_TIME                  5000
#define BUZZER_DISABLED_TIME                3000

// Comm
#define SECONDS_TO_REMEMBER_PEERS           30
#define ACCOUNCEMENT_INTERVAL_SECONDS       10
#define SHUTDOWN_TIME_NO_BUZZING_SECONDS    (60 * 20)   // 20 minutes without buzzing, even when others are around -> shut down
#define SHUTDOWN_TIME_NO_COMMS_SECONDS      (60 * 5)    // 5 minutes without another nearby buzzer -> shutdown
#define PING_INTERVAL                       0           // No pinging

// Led
#define NUM_LEDS                            14

#define SHUTDOWN_ANIMATION_DURATION         1000
#define FLASH_EFFECT_COLOR                  CRGB::White // CRGB::White // baseColor
#define FLASH_EFFECT_PRE_FLASH_COUNT        4
#define FLASH_EFFECT_PRE_FLASH_DURATION     100
#define FLASH_EFFECT_DURATION               600