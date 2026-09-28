#pragma once
#include "interfaces.h"
#include <Adafruit_PN532.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>

class NfcReader : public INfc {
public:
  NfcReader();
  bool begin() override;
  bool poll(TagState &tag) override;
  void stop() override;
private:
  static void taskEntry(void *instance);
  void scan();
  bool read(TagState &tag);
  Adafruit_PN532 device_;
  QueueHandle_t queue_ = nullptr;
  std::atomic<bool> stopRequested_{false};
};