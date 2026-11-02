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

constexpr uint32_t BATTERY_DIVIDER_TOP_OHMS = 100000;
constexpr uint32_t BATTERY_DIVIDER_BOTTOM_OHMS = 47000;
constexpr uint32_t BATTERY_CALIBRATION_PERMILLE = 1000;
constexpr uint8_t BATTERY_ADC_SAMPLES = 16;
constexpr uint32_t BATTERY_SAMPLE_MS = 250;
constexpr uint32_t BATTERY_LOG_MS = 5000;
constexpr uint32_t BATTERY_START_MIN_MV = 3500;
constexpr uint32_t BATTERY_LOW_MV = 3300;
constexpr uint32_t BATTERY_CRITICAL_MV = 3000;
constexpr uint32_t BATTERY_MAX_VALID_MV = 4350;
constexpr uint32_t BATTERY_LOW_HOLD_MS = 2000;
constexpr uint32_t BATTERY_SOFT_HOLD_MS = 5000;
constexpr char LOW_BATTERY_FILE[] = "low_battery.mp3";
constexpr uint8_t LOW_BATTERY_VOLUME = 10;
constexpr uint32_t LOW_BATTERY_MAX_MS = 10000;

constexpr uint8_t MOTOR_PWM_CHANNEL = 0;
constexpr uint32_t MOTOR_PWM_HZ = 20000;
constexpr uint8_t MOTOR_PWM_BITS = 8;
constexpr uint8_t MOTOR_START_DUTY = 60;
constexpr uint8_t MOTOR_RUN_DUTY = 230;
constexpr uint32_t MOTOR_RAMP_MS = 600;
constexpr uint32_t MOTOR_UPDATE_MS = 15;

constexpr size_t AUDIO_BUFFER_BYTES = 8192;
constexpr uint32_t AUDIO_FADE_MS = 1000;
constexpr uint8_t VOLUME_SCALE = 21;
constexpr uint8_t INITIAL_VOLUME = 10;
constexpr uint8_t MIN_VOLUME = 0;
constexpr uint8_t MAX_VOLUME = 15;

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
static_assert(BATTERY_DIVIDER_TOP_OHMS > 0 && BATTERY_DIVIDER_BOTTOM_OHMS > 0 &&
              BATTERY_CALIBRATION_PERMILLE >= 800 && BATTERY_CALIBRATION_PERMILLE <= 1200,
              "Check battery divider and calibration");
static_assert(BATTERY_CRITICAL_MV > 0 && BATTERY_CRITICAL_MV < BATTERY_LOW_MV &&
              BATTERY_LOW_MV < BATTERY_START_MIN_MV && BATTERY_START_MIN_MV < BATTERY_MAX_VALID_MV,
              "Invalid battery voltage thresholds");
static_assert(BATTERY_ADC_SAMPLES > 0 && BATTERY_SAMPLE_MS > 0 &&
              BATTERY_LOW_HOLD_MS >= BATTERY_SAMPLE_MS && BATTERY_LOG_MS >= BATTERY_SAMPLE_MS,
              "Invalid battery sampling/timing");
static_assert(BATTERY_SOFT_HOLD_MS >= BATTERY_SAMPLE_MS && LOW_BATTERY_MAX_MS > AUDIO_FADE_MS &&
              LOW_BATTERY_VOLUME > 0 && LOW_BATTERY_VOLUME <= MAX_VOLUME,
              "Invalid low-battery warning settings");
static_assert(MIN_VOLUME <= INITIAL_VOLUME && INITIAL_VOLUME <= MAX_VOLUME && MAX_VOLUME > 0,
              "Invalid volume settings");
static_assert(MAX_VOLUME <= VOLUME_SCALE && VOLUME_SCALE > 0,
              "Selectable volume must not exceed full-scale gain reference");
static_assert(LED_COUNT > 0 && MAX_PLAYS > 0 && NFC_MISSING_READS > 0, "Counts must be positive");
static_assert(LED_START_BRIGHTNESS <= LED_BRIGHTNESS, "LED startup brightness must not exceed steady brightness");
static_assert((LED_PATTERN_MASK & 0x0FFF) != 0 && (LED_PATTERN_MASK & 0xF000) == 0,
              "Enable at least one of the 12 LED patterns");
}