#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Config {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t SD_SPEED_HZ = 10000000;
constexpr char TRACK_DIRECTORY[] = "/player";
constexpr uint8_t MAX_PLAYS = 3;
constexpr size_t UID_CAPACITY = 15;
constexpr size_t TRACK_PATH_CAPACITY = 40;

constexpr uint32_t BOOT_SD_EARLIEST_MS = 200;
constexpr uint32_t BOOT_NFC_EARLIEST_MS = 400;
constexpr uint32_t BOOT_READY_EARLIEST_MS = 600;
constexpr uint32_t IDLE_SLEEP_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr uint32_t BUTTON_HOLD_MS = 1500;
constexpr uint32_t ENCODER_DEBOUNCE_MS = 2;

constexpr bool KEY_KEEPALIVE_ENABLED = true;
constexpr uint32_t KEY_INTERVAL_MS = 20000;
constexpr uint32_t KEY_PULSE_MS = 120;

constexpr uint8_t MOTOR_PWM_CHANNEL = 0;
constexpr uint32_t MOTOR_PWM_HZ = 20000;
constexpr uint8_t MOTOR_PWM_BITS = 8;
constexpr uint8_t MOTOR_START_DUTY = 60;
constexpr uint8_t MOTOR_RUN_DUTY = 230;
constexpr uint32_t MOTOR_RAMP_MS = 600;
constexpr uint32_t MOTOR_UPDATE_MS = 15;

constexpr size_t AUDIO_BUFFER_BYTES = 8192;
constexpr uint32_t AUDIO_FADE_MS = 1000;
constexpr uint8_t INITIAL_VOLUME = 12;
constexpr uint8_t MIN_VOLUME = 0;
constexpr uint8_t MAX_VOLUME = 21;

constexpr uint8_t LED_COUNT = 24;
constexpr uint8_t LED_BRIGHTNESS = 100;
constexpr uint8_t LED_START_BRIGHTNESS = 20;
constexpr uint32_t LED_UPDATE_MS = 20;
constexpr uint32_t LED_START_MS = 1000;
constexpr uint32_t LED_STOP_MS = 500;
constexpr uint16_t LED_PATTERN_MASK = 0x0FFF;

constexpr uint32_t NFC_SCAN_MS = 150;
constexpr uint16_t NFC_READ_TIMEOUT_MS = 50;
constexpr uint8_t NFC_MISSING_READS = 2;
constexpr uint8_t NFC_RETRY_COUNT = 1;
constexpr uint32_t NFC_TASK_STACK_BYTES = 4096;

static_assert(BOOT_SD_EARLIEST_MS <= BOOT_NFC_EARLIEST_MS &&
              BOOT_NFC_EARLIEST_MS <= BOOT_READY_EARLIEST_MS, "Boot stages must be ordered");
static_assert(MOTOR_PWM_BITS == 8 && MOTOR_START_DUTY <= MOTOR_RUN_DUTY,
              "Use 8-bit PWM and START <= RUN <= 255");
static_assert(MOTOR_RAMP_MS > 0 && AUDIO_FADE_MS > 0 && LED_START_MS > 0 && LED_STOP_MS > 0,
              "Fade/ramp durations must be positive");
static_assert(KEY_PULSE_MS > 0 && KEY_PULSE_MS < KEY_INTERVAL_MS, "Invalid KEY timing");
static_assert(MIN_VOLUME <= INITIAL_VOLUME && INITIAL_VOLUME <= MAX_VOLUME && MAX_VOLUME > 0,
              "Invalid volume settings");
static_assert(LED_COUNT > 0 && MAX_PLAYS > 0 && NFC_MISSING_READS > 0, "Counts must be positive");
static_assert(LED_START_BRIGHTNESS <= LED_BRIGHTNESS, "LED startup brightness must not exceed steady brightness");
static_assert((LED_PATTERN_MASK & 0x0FFF) != 0 && (LED_PATTERN_MASK & 0xF000) == 0,
              "Enable at least one of the 12 LED patterns");
}