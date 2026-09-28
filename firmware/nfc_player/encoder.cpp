#include "encoder.h"
#include "pins.h"
#include <Arduino.h>
#include <driver/rtc_io.h>

void EncoderInput::begin() {
  rtc_gpio_deinit(static_cast<gpio_num_t>(Pins::ENCODER_SW));
  pinMode(Pins::ENCODER_CLK, INPUT_PULLUP);
  pinMode(Pins::ENCODER_DT, INPUT_PULLUP);
  pinMode(Pins::ENCODER_SW, INPUT_PULLUP);
  lastClock_ = digitalRead(Pins::ENCODER_CLK);
}

bool EncoderInput::pressed() const { return digitalRead(Pins::ENCODER_SW) == LOW; }

int8_t EncoderInput::delta(uint32_t now) {
  int clock = digitalRead(Pins::ENCODER_CLK);
  int8_t change = 0;
  if (clock == LOW && lastClock_ == HIGH && now - lastTurn_ >= Config::ENCODER_DEBOUNCE_MS) {
    lastTurn_ = now;
    change = digitalRead(Pins::ENCODER_DT) != clock ? 1 : -1;
  }
  lastClock_ = clock;
  return change;
}