#pragma once
#include "interfaces.h"

enum class PlayerState { BootSD, BootNfc, BootReady, Idle, MotorRamp, Playing, WaitRelease, Sleeping, Fault };

class Player {
public:
  Player(IPower &power, IMotor &motor, ILeds &leds, IAudio &audio,
         INfc &nfc, IStorage &storage, IEncoder &encoder)
    : Power(power), Motor(motor), Leds(leds), Audio(audio), Nfc(nfc), Storage(storage), Encoder(encoder) {}
  void begin(uint32_t now);
  void tick(uint32_t now);
  PlayerState state() const { return state_; }
  const char *error() const { return error_; }
  uint8_t volume() const { return volume_; }
  uint8_t completedPlays() const { return plays_; }
private:
  void button(uint32_t now);
  void requestSleep(uint32_t now);
  void sleep();
  void stopTrack(uint32_t now, LedMode mode);
  void startTrack(uint32_t now);
  void fail(const char *message, uint32_t now);
  IPower &Power;
  IMotor &Motor;
  ILeds &Leds;
  IAudio &Audio;
  INfc &Nfc;
  IStorage &Storage;
  IEncoder &Encoder;
  PlayerState state_ = PlayerState::BootSD;
  const char *error_ = "";
  uint32_t bootAt_ = 0, lastActivity_ = 0, buttonChanged_ = 0, pressedAt_ = 0;
  bool buttonRaw_ = false, buttonStable_ = false, buttonArmed_ = true;
  uint8_t volume_ = Config::INITIAL_VOLUME, plays_ = 0;
  char uid_[Config::UID_CAPACITY] = {};
  char path_[Config::TRACK_PATH_CAPACITY] = {};
};