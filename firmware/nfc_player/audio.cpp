#include "audio.h"
#include "pins.h"
#include <Arduino.h>
#include <new>

bool AudioPlayer::begin() {
  output_ = new (std::nothrow) AudioOutputI2S();
  if (output_ == nullptr) return false;
  output_->SetGain(0.0f);
  return output_->SetPinout(Pins::I2S_BCLK, Pins::I2S_LRC, Pins::I2S_DOUT);
}

bool AudioPlayer::play(const char *path, uint8_t volume) {
  stop();
  if (output_ == nullptr) return false;
  volume_ = volume;
  file_ = new (std::nothrow) AudioFileSourceSD(path);
  if (file_ != nullptr && file_->isOpen()) buffer_ = new (std::nothrow) AudioFileSourceBuffer(file_, Config::AUDIO_BUFFER_BYTES);
  if (buffer_ != nullptr) decoder_ = new (std::nothrow) AudioGeneratorMP3();
  if (decoder_ == nullptr || !decoder_->begin(buffer_, output_)) { stop(); return false; }
  output_->SetGain(0.0f);
  fadeAt_ = millis();
  running_ = true;
  digitalWrite(Pins::AMP_ENABLE, HIGH);
  return true;
}

void AudioPlayer::applyGain(uint32_t now) {
  if (output_ == nullptr) return;
  if (!running_) { output_->SetGain(0.0f); return; }
  uint32_t elapsed = now - fadeAt_;
  float fade = elapsed >= Config::AUDIO_FADE_MS ? 1.0f : static_cast<float>(elapsed) / Config::AUDIO_FADE_MS;
  float level = static_cast<float>(volume_) / Config::MAX_VOLUME;
  output_->SetGain(level * level * fade);
}

void AudioPlayer::volume(uint8_t value) { volume_ = value; applyGain(millis()); }

bool AudioPlayer::tick(uint32_t now) {
  if (!running_ || decoder_ == nullptr) return false;
  applyGain(now);
  if (!decoder_->isRunning() || !decoder_->loop()) { stop(); return false; }
  return true;
}

void AudioPlayer::stop() {
  digitalWrite(Pins::AMP_ENABLE, LOW);
  running_ = false;
  if (output_ != nullptr) output_->SetGain(0.0f);
  if (decoder_ != nullptr) {
    if (decoder_->isRunning()) decoder_->stop();
    delete decoder_;
    decoder_ = nullptr;
  }
  delete buffer_;
  buffer_ = nullptr;
  if (file_ != nullptr) { file_->close(); delete file_; file_ = nullptr; }
}