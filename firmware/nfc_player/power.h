#pragma once
#include "interfaces.h"
#include <esp_timer.h>

class PowerControl : public IPower {
public:
  bool begin() override;
  void tick(uint32_t now) override;
  void disableKeepAlive() override;
  void sleep() override;
private:
  static void releaseKey(void *argument);
  esp_timer_handle_t releaseTimer_ = nullptr;
  uint32_t lastPulse_ = 0;
  bool enabled_ = false;
};