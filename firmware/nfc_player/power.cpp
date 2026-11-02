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
  for (uint8_t pin : {Pins::MOTOR_GATE, Pins::AMP_ENABLE}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  Serial.begin(Config::SERIAL_BAUD);
  Serial.printf("Reset reason: %d; wake cause: %d\n",
                static_cast<int>(esp_reset_reason()), static_cast<int>(esp_sleep_get_wakeup_cause()));
  pinMode(Pins::BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(Pins::BATTERY_ADC, ADC_6db);
  delay(20);
  sampleBattery();
  sampledAt_ = loggedAt_ = millis();
  logBattery();
  return true;
}

void PowerControl::sampleBattery() {
  uint32_t total = 0;
  for (uint8_t sample = 0; sample < Config::BATTERY_ADC_SAMPLES; ++sample) {
    total += analogReadMilliVolts(Pins::BATTERY_ADC);
  }
  adcMillivolts_ = (total + Config::BATTERY_ADC_SAMPLES / 2) / Config::BATTERY_ADC_SAMPLES;
  batteryMillivolts_ = batteryMillivoltsFromAdc(adcMillivolts_);
}

void PowerControl::logBattery() const {
  Serial.printf("Battery ADC: %lu mV; pack: %lu mV\n",
                static_cast<unsigned long>(adcMillivolts_), static_cast<unsigned long>(batteryMillivolts_));
}

void PowerControl::tick(uint32_t now) {
  if (now - sampledAt_ >= Config::BATTERY_SAMPLE_MS) {
    sampledAt_ = now;
    sampleBattery();
  }
  if (now - loggedAt_ >= Config::BATTERY_LOG_MS) {
    loggedAt_ = now;
    logBattery();
  }
}

void PowerControl::sleep(bool allowButtonWake) {
  digitalWrite(Pins::AMP_ENABLE, LOW);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (allowButtonWake) {
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(Pins::ENCODER_SW));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(Pins::ENCODER_SW));
    if (esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(Pins::ENCODER_SW), LOW) != ESP_OK) {
      Serial.println("Wake-source configuration failed; restart required");
      return;
    }
  } else {
    Serial.println("Battery voltage unsafe; recharge and power-cycle. Button wake disabled.");
  }
  Serial.println("Deep sleep; peripheral power is not disconnected");
  Serial.flush();
  esp_deep_sleep_start();
}