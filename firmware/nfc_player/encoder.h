#pragma once
#include "interfaces.h"

class EncoderInput : public IEncoder {
public:
  void begin() override;
  bool pressed() const override;
  int8_t delta(uint32_t now) override;
private:
  int lastClock_ = 1;
  uint32_t lastTurn_ = 0;
};