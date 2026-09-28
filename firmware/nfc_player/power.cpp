#include "power.h"
#include "pins.h"
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <initializer_list>

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error This wiring targets the classic ESP32 WROOM DevKit V1, not C3/S3/WROVER.
#endif

bool PowerControl::begin() {
  for (uint8_t pin : {Pins::MOTOR_GATE, Pins::AMP_ENABLE, Pins::IP5310_KEY}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  Serial.begin(Config::SERIAL_BAUD);
  Serial.printf("Reset reason: %d; wake cause: %d\n",
                static_cast<int>(esp_reset_reason()), static_cast<int>(esp_sleep_get_wakeup_cause()));
  lastPulse_ = millis();
  if (!Config::KEY_KEEPALIVE_ENABLED) return true;
  esp_timer_create_args_t args = {};
  args.callback = releaseKey;
  args.name = "key_release";
  if (esp_timer_create(&args, &releaseTimer_) != ESP_OK) return false;
  enabled_ = true;
  return true;
}

void PowerControl::releaseKey(void *) { digitalWrite(Pins::IP5310_KEY, LOW); }

void PowerControl::tick(uint32_t now) {
  if (!enabled_ || esp_timer_is_active(releaseTimer_)) return;
  if (now - lastPulse_ < Config::KEY_INTERVAL_MS) return;
  lastPulse_ = now;
  digitalWrite(Pins::IP5310_KEY, HIGH);
  if (esp_timer_start_once(releaseTimer_, static_cast<uint64_t>(Config::KEY_PULSE_MS) * 1000ULL) != ESP_OK) {
    releaseKey(nullptr);
    Serial.println("KEY release timer failed; pulse cancelled");
  }
}

void PowerControl::disableKeepAlive() {
  enabled_ = false;
  if (releaseTimer_ != nullptr) esp_timer_stop(releaseTimer_);
  releaseKey(nullptr);
}

void PowerControl::sleep() {
  disableKeepAlive();
  digitalWrite(Pins::AMP_ENABLE, LOW);
  rtc_gpio_pullup_en(static_cast<gpio_num_t>(Pins::ENCODER_SW));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(Pins::ENCODER_SW));
  if (esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(Pins::ENCODER_SW), LOW) != ESP_OK) {
    Serial.println("Wake-source configuration failed; restart required");
    return;
  }
  Serial.println("Deep sleep; peripheral power is not disconnected");
  Serial.flush();
  esp_deep_sleep_start();
}