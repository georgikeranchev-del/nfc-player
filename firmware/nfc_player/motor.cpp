#include "motor.h"
#include "pins.h"
#include <Arduino.h>
#include <esp_arduino_version.h>

bool Esp32Pwm::begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ledcAttach(Pins::MOTOR_GATE, Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS);
#else
  if (ledcSetup(Config::MOTOR_PWM_CHANNEL, Config::MOTOR_PWM_HZ, Config::MOTOR_PWM_BITS) == 0) return false;
  ledcAttachPin(Pins::MOTOR_GATE, Config::MOTOR_PWM_CHANNEL);
  return true;
#endif
}

void Esp32Pwm::write(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(Pins::MOTOR_GATE, duty);
#else
  ledcWrite(Config::MOTOR_PWM_CHANNEL, duty);
#endif
}