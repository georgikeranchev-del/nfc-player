#pragma once
#include "interfaces.h"
#include <FastLED.h>

class LedRing : public ILeds {
public:
  void begin() override;
  void mode(LedMode mode, uint32_t now) override;
  void selectPattern() override;
  void tick(uint32_t now) override;
private:
  void pattern(uint32_t now);
  CRGB pixels_[Config::LED_COUNT];
  LedMode mode_ = LedMode::Off;
  uint32_t modeAt_ = 0, lastUpdate_ = 0;
  uint8_t pattern_ = 0, lastPattern_ = 255;
  bool initialized_ = false;
};