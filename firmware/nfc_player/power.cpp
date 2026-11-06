#include "power.h"
#include "pins.h"
#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <initializer_list>

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error This wiring targets the classic ESP32 WROOM DevKit V1, not C3/S3/WROVER.
#endif

namespace {
RTC_DATA_ATTR bool playbackInhibited = false;
}

bool PowerControl::begin() {
  for (uint8_t pin : {Pins::PERIPH_POWER, Pins::MOTOR_GATE, Pins::AMP_ENABLE}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  Serial.begin(Config::SERIAL_BAUD);
  Serial.printf("Reset reason: %d; wake cause: %d\n",
                static_cast<int>(esp_reset_reason()), static_cast<int>(esp_sleep_get_wakeup_cause()));
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) ::playbackInhibited = false;
  pinMode(Pins::BATTERY_ADC, INPUT);
  pinMode(Pins::CHARGER_PRESENT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(Pins::BATTERY_ADC, ADC_6db);
  delay(20);
  sampleInputs();
  sampledAt_ = loggedAt_ = millis();
  logBattery();
  return true;
}

void PowerControl::enablePeripherals() {
  digitalWrite(Pins::PERIPH_POWER, HIGH);
  delay(Config::PERIPHERAL_SETTLE_MS);
  Serial.println("Peripheral 5 V rail on");
}

void PowerControl::sampleInputs() {
  chargerPresent_ = digitalRead(Pins::CHARGER_PRESENT) == LOW;
  uint32_t total = 0;
  for (uint8_t sample = 0; sample < Config::BATTERY_ADC_SAMPLES; ++sample) {
    total += analogReadMilliVolts(Pins::BATTERY_ADC);
  }
  adcMillivolts_ = (total + Config::BATTERY_ADC_SAMPLES / 2) / Config::BATTERY_ADC_SAMPLES;
  batteryMillivolts_ = batteryMillivoltsFromAdc(adcMillivolts_);
}

void PowerControl::logBattery() const {
  Serial.printf("Battery ADC: %lu mV; pack: %lu mV; charger: %s\n",
                static_cast<unsigned long>(adcMillivolts_), static_cast<unsigned long>(batteryMillivolts_),
                chargerPresent_ ? "present" : "absent");
}

bool PowerControl::playbackInhibited() const { return ::playbackInhibited; }

void PowerControl::inhibitPlayback(bool inhibited) {
  if (::playbackInhibited == inhibited) return;
  ::playbackInhibited = inhibited;
  Serial.println(inhibited ? "Low battery: songs inhibited" : "Battery recovered: songs enabled");
}

void PowerControl::tick(uint32_t now) {
  if (now - sampledAt_ >= Config::BATTERY_SAMPLE_MS) {
    sampledAt_ = now;
    sampleInputs();
  }
  if (now - loggedAt_ >= Config::BATTERY_LOG_MS) {
    loggedAt_ = now;
    logBattery();
  }
}

void PowerControl::sleep(bool allowButtonWake, bool allowChargerWake) {
  digitalWrite(Pins::AMP_ENABLE, LOW);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (allowButtonWake) {
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(Pins::ENCODER_SW));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(Pins::ENCODER_SW));
    if (esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(Pins::ENCODER_SW), LOW) != ESP_OK) {
      Serial.println("Wake-source configuration failed; restart required");
      return;
    }
  } else if (allowChargerWake) {
    // Level-triggered ext0 would re-wake at once while USB stays present, so poll by timer then.
    if (chargerPresent_) {
      esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(Config::CHARGER_WAKE_POLL_MS) * 1000ULL);
    } else if (esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(Pins::CHARGER_PRESENT), LOW) != ESP_OK) {
      Serial.println("Charger wake configuration failed; restart required");
      return;
    }
    Serial.println("Battery voltage unsafe; button wake disabled; waking for charger.");
  } else {
    Serial.println("Battery voltage unsafe; recharge and power-cycle. Button wake disabled.");
  }
  Serial.println("Deep sleep; peripheral 5 V rail off");
  Serial.flush();
  // Release signal pins first so no GPIO back-powers a module through its ESD diodes as the rail decays.
  for (uint8_t pin : {Pins::SD_CS, Pins::SD_SCK, Pins::SD_MISO, Pins::SD_MOSI, Pins::PN532_SS,
                      Pins::PN532_SCK, Pins::PN532_MISO, Pins::PN532_MOSI, Pins::I2S_LRC,
                      Pins::I2S_BCLK, Pins::I2S_DOUT, Pins::LED_DATA}) {
    pinMode(pin, INPUT);
  }
  digitalWrite(Pins::MOTOR_GATE, LOW);
  digitalWrite(Pins::PERIPH_POWER, LOW);
  esp_deep_sleep_start();
}