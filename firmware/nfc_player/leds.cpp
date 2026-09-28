#include "leds.h"
#include "pins.h"
#include <Arduino.h>
#include <esp_system.h>

void LedRing::begin() {
  FastLED.addLeds<WS2812B, Pins::LED_DATA, GRB>(pixels_, Config::LED_COUNT);
  FastLED.setBrightness(Config::LED_BRIGHTNESS);
  random16_set_seed(static_cast<uint16_t>(esp_random()));
  initialized_ = true;
  mode(LedMode::Off, millis());
}

void LedRing::mode(LedMode value, uint32_t now) {
  mode_ = value;
  modeAt_ = now;
  if (initialized_ && mode_ == LedMode::Off) {
    fill_solid(pixels_, Config::LED_COUNT, CRGB::Black);
    FastLED.show();
  }
}

void LedRing::selectPattern() {
  uint8_t choices[12] = {}, count = 0;
  for (uint8_t index = 0; index < 12; ++index) {
    if ((Config::LED_PATTERN_MASK & (1U << index)) != 0 && index != lastPattern_) choices[count++] = index;
  }
  if (count > 0) pattern_ = choices[esp_random() % count];
  lastPattern_ = pattern_;
  Serial.printf("LED pattern: %u\n", pattern_);
}

void LedRing::tick(uint32_t now) {
  if (!initialized_ || mode_ == LedMode::Off || now - lastUpdate_ < Config::LED_UPDATE_MS) return;
  lastUpdate_ = now;
  uint32_t elapsed = now - modeAt_;
  FastLED.setBrightness(Config::LED_BRIGHTNESS);
  switch (mode_) {
    case LedMode::Standby: {
      uint8_t breath = beatsin8(6, 3, 40);
      fill_solid(pixels_, Config::LED_COUNT, CRGB(breath / 4, breath / 2, breath));
      break;
    }
    case LedMode::Starting: {
      uint32_t bounded = elapsed < Config::LED_START_MS ? elapsed : Config::LED_START_MS;
      FastLED.setBrightness(static_cast<uint8_t>(Config::LED_START_BRIGHTNESS +
        (Config::LED_BRIGHTNESS - Config::LED_START_BRIGHTNESS) * bounded / Config::LED_START_MS));
      fill_solid(pixels_, Config::LED_COUNT, CRGB::Black);
      uint8_t count = static_cast<uint8_t>(bounded * Config::LED_COUNT / Config::LED_START_MS);
      for (uint8_t index = 0; index < count; ++index) {
        uint8_t brightness = static_cast<uint8_t>(60U + index * 195U / count);
        pixels_[index] = CRGB(brightness / 2, brightness, 255);
      }
      if (elapsed >= Config::LED_START_MS) mode(LedMode::Playing, now);
      break;
    }
    case LedMode::Playing: pattern(now); break;
    case LedMode::Stopping:
      if (elapsed >= Config::LED_STOP_MS) {
        fill_solid(pixels_, Config::LED_COUNT, CRGB::Black);
        mode(LedMode::Standby, now);
      } else {
        uint8_t brightness = static_cast<uint8_t>(255U - elapsed * 255U / Config::LED_STOP_MS);
        for (auto &pixel : pixels_) pixel.nscale8(brightness);
      }
      break;
    case LedMode::Off: break;
  }
  FastLED.show();
}

void LedRing::pattern(uint32_t now) {
  const uint8_t count = Config::LED_COUNT;
  auto behind = [count](uint16_t position, uint8_t offset) { return (position + count - offset % count) % count; };
  switch (pattern_) {
    case 0: {
      fadeToBlackBy(pixels_, count, 35);
      uint16_t position = (now / 70) % count;
      pixels_[position] += CRGB(120, 180, 255);
      pixels_[behind(position, 1)] += CRGB(50, 80, 120);
      pixels_[behind(position, 2)] += CRGB(20, 35, 60);
      break;
    }
    case 1: {
      fadeToBlackBy(pixels_, count, 50);
      uint16_t position = (now / 55) % count;
      pixels_[position] = CRGB(180, 220, 255);
      pixels_[behind(position, 1)] += CRGB(100, 130, 180);
      pixels_[behind(position, 2)] += CRGB(45, 60, 100);
      pixels_[behind(position, 3)] += CRGB(15, 20, 40);
      break;
    }
    case 2: {
      fadeToBlackBy(pixels_, count, 42);
      uint16_t position = (now / 70) % count, opposite = (position + count / 2) % count;
      pixels_[position] = CRGB(120, 200, 255);
      pixels_[opposite] = CRGB(255, 100, 180);
      pixels_[behind(position, 1)] += CRGB(50, 80, 120);
      pixels_[(opposite + 1) % count] += CRGB(100, 40, 80);
      break;
    }
    case 3: {
      uint8_t wave = beatsin8(18, 35, 150);
      fill_solid(pixels_, count, CRGB(wave / 2, wave, 255));
      uint16_t position = (now / 90) % count;
      pixels_[position] = CRGB::White;
      pixels_[behind(position, 1)] = CRGB(120, 180, 220);
      break;
    }
    case 4: {
      fadeToBlackBy(pixels_, count, 45);
      uint16_t position = (now / 60) % count;
      pixels_[position] = CRGB(180, 230, 255);
      pixels_[behind(position, 1)] += CRGB(90, 130, 180);
      pixels_[behind(position, 2)] += CRGB(40, 60, 90);
      if (random8() < 35) pixels_[random8(count)] += CRGB(180, 180, 255);
      break;
    }
    case 5: {
      fadeToBlackBy(pixels_, count, 20);
      uint8_t brightness = beatsin8(12, 35, 180);
      uint16_t position = (now / 95) % count;
      pixels_[position] = CRGB(brightness / 2, brightness, brightness);
      pixels_[(position + 1) % count] += CRGB(20, 40, 50);
      pixels_[behind(position, 1)] += CRGB(20, 40, 50);
      break;
    }
    case 6: {
      fadeToBlackBy(pixels_, count, 55);
      uint16_t phase = (now / 120) % count;
      pixels_[phase] += CRGB(120, 180, 255);
      pixels_[(count - phase) % count] += CRGB(120, 180, 255);
      break;
    }
    case 7: {
      fadeToBlackBy(pixels_, count, 38);
      uint16_t position = (now / 80) % count, opposite = (position + count / 2) % count;
      pixels_[position] = CRGB(255, 80, 120);
      pixels_[opposite] = CRGB(80, 160, 255);
      pixels_[behind(position, 1)] += CRGB(100, 30, 50);
      pixels_[behind(opposite, 1)] += CRGB(30, 60, 100);
      break;
    }
    case 8: {
      fadeToBlackBy(pixels_, count, 42);
      uint16_t center = (now / 70) % count;
      pixels_[center] = CRGB(220, 240, 255);
      pixels_[(center + 1) % count] += CRGB(100, 150, 200);
      pixels_[behind(center, 1)] += CRGB(100, 150, 200);
      pixels_[(center + 2) % count] += CRGB(40, 70, 100);
      pixels_[behind(center, 2)] += CRGB(40, 70, 100);
      break;
    }
    case 9:
      fadeToBlackBy(pixels_, count, 25);
      if (random8() < 70) pixels_[random8(count)] += CRGB(random8(60, 150), random8(80, 180), 255);
      break;
    case 10:
      for (uint8_t index = 0; index < count; ++index) {
        uint8_t wave = sin8(static_cast<uint8_t>(index * 255U / count + now / 18));
        uint8_t brightness = scale8(wave, 150);
        pixels_[index] = CRGB(brightness / 3, brightness, brightness);
      }
      break;
    case 11: {
      fadeToBlackBy(pixels_, count, 35);
      uint16_t position = (now / 90) % count;
      uint8_t pulse = beatsin8(28, 50, 220);
      pixels_[position] = CRGB(pulse, pulse / 3, pulse / 2);
      pixels_[behind(position, 1)] += CRGB(pulse / 3, pulse / 8, pulse / 6);
      break;
    }
  }
}