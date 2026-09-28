#pragma once
#include "interfaces.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

class AudioPlayer : public IAudio {
public:
  bool begin() override;
  bool play(const char *path, uint8_t volume) override;
  bool tick(uint32_t now) override;
  void volume(uint8_t value) override;
  void stop() override;
private:
  void applyGain(uint32_t now);
  AudioOutputI2S *output_ = nullptr;
  AudioFileSourceSD *file_ = nullptr;
  AudioFileSourceBuffer *buffer_ = nullptr;
  AudioGeneratorMP3 *decoder_ = nullptr;
  uint32_t fadeAt_ = 0;
  uint8_t volume_ = Config::INITIAL_VOLUME;
  bool running_ = false;
};