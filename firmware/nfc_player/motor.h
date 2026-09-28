#pragma once
#include "interfaces.h"

class Esp32Pwm : public IPwm {
public:
  bool begin() override;
  void write(uint8_t duty) override;
};

class MotorControl : public IMotor {
public:
  explicit MotorControl(IPwm &pwm) : pwm_(pwm) {}
  bool begin() override { initialized_ = pwm_.begin(); stop(); return initialized_; }
  void start(uint32_t now) override {
    if (!initialized_) return;
    start_ = lastUpdate_ = now;
    running_ = true;
    ramping_ = true;
    pwm_.write(Config::MOTOR_START_DUTY);
  }
  void tick(uint32_t now) override {
    if (!ramping_) return;
    uint32_t elapsed = now - start_;
    if (elapsed >= Config::MOTOR_RAMP_MS) {
      pwm_.write(Config::MOTOR_RUN_DUTY);
      ramping_ = false;
    } else if (now - lastUpdate_ >= Config::MOTOR_UPDATE_MS) {
      lastUpdate_ = now;
      uint32_t duty = Config::MOTOR_START_DUTY +
        (Config::MOTOR_RUN_DUTY - Config::MOTOR_START_DUTY) * elapsed / Config::MOTOR_RAMP_MS;
      pwm_.write(static_cast<uint8_t>(duty));
    }
  }
  void stop() override {
    if (initialized_) pwm_.write(0);
    running_ = ramping_ = false;
  }
  bool ready() const override { return running_ && !ramping_; }
private:
  IPwm &pwm_;
  bool initialized_ = false, running_ = false, ramping_ = false;
  uint32_t start_ = 0, lastUpdate_ = 0;
};