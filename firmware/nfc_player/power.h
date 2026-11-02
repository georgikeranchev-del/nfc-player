#pragma once
#include "interfaces.h"

class PowerControl : public IPower {
public:
  bool begin() override;
  void tick(uint32_t now) override;
  uint32_t batteryMillivolts() const override { return batteryMillivolts_; }
  void sleep(bool allowButtonWake) override;
  static constexpr uint32_t batteryMillivoltsFromAdc(uint32_t adcMillivolts,
      uint32_t calibrationPermille = Config::BATTERY_CALIBRATION_PERMILLE) {
    return static_cast<uint32_t>((static_cast<uint64_t>(adcMillivolts) *
      (Config::BATTERY_DIVIDER_TOP_OHMS + Config::BATTERY_DIVIDER_BOTTOM_OHMS) *
      calibrationPermille + static_cast<uint64_t>(Config::BATTERY_DIVIDER_BOTTOM_OHMS) * 500) /
      (static_cast<uint64_t>(Config::BATTERY_DIVIDER_BOTTOM_OHMS) * 1000));
  }
private:
  void sampleBattery();
  void logBattery() const;
  uint32_t sampledAt_ = 0, loggedAt_ = 0;
  uint32_t adcMillivolts_ = 0, batteryMillivolts_ = 0;
};