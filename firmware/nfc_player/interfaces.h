#pragma once
#include "config.h"

struct TagState { char uid[Config::UID_CAPACITY] = {}; };
enum class LedMode { Off, Standby, Starting, Playing, Stopping };

struct IPower {
  virtual ~IPower() = default;
  virtual bool begin() = 0;
  virtual void tick(uint32_t now) = 0;
  virtual void disableKeepAlive() = 0;
  virtual void sleep() = 0;
};
struct IPwm {
  virtual ~IPwm() = default;
  virtual bool begin() = 0;
  virtual void write(uint8_t duty) = 0;
};
struct IMotor {
  virtual ~IMotor() = default;
  virtual bool begin() = 0;
  virtual void start(uint32_t now) = 0;
  virtual void tick(uint32_t now) = 0;
  virtual void stop() = 0;
  virtual bool ready() const = 0;
};
struct ILeds {
  virtual ~ILeds() = default;
  virtual void begin() = 0;
  virtual void mode(LedMode mode, uint32_t now) = 0;
  virtual void selectPattern() = 0;
  virtual void tick(uint32_t now) = 0;
};
struct IAudio {
  virtual ~IAudio() = default;
  virtual bool begin() = 0;
  virtual bool play(const char *path, uint8_t volume) = 0;
  virtual bool tick(uint32_t now) = 0;
  virtual void volume(uint8_t value) = 0;
  virtual void stop() = 0;
};
struct INfc {
  virtual ~INfc() = default;
  virtual bool begin() = 0;
  virtual bool poll(TagState &tag) = 0;
  virtual void stop() = 0;
};
struct IStorage {
  virtual ~IStorage() = default;
  virtual bool begin() = 0;
  virtual bool exists(const char *path) = 0;
};
struct IEncoder {
  virtual ~IEncoder() = default;
  virtual void begin() = 0;
  virtual bool pressed() const = 0;
  virtual int8_t delta(uint32_t now) = 0;
};