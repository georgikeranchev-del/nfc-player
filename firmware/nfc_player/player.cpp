#include "player.h"
#include <stdio.h>
#include <string.h>

void Player::begin(uint32_t now) {
  bootAt_ = lastActivity_ = buttonChanged_ = now;
  bool powerReady = Power.begin();
  Encoder.begin();
  buttonRaw_ = buttonStable_ = Encoder.pressed();
  buttonArmed_ = !buttonRaw_;
  Leds.begin();
  Leds.mode(LedMode::Off, now);
  if (!powerReady) { fail("Power timer initialization failed", now); return; }
  if (!Motor.begin()) { fail("Motor PWM initialization failed", now); return; }
  state_ = PlayerState::BootSD;
}

void Player::stopTrack(uint32_t now, LedMode mode) {
  Motor.stop();
  Audio.stop();
  Leds.mode(mode, now);
}

void Player::fail(const char *message, uint32_t now) {
  error_ = message;
  Power.disableKeepAlive();
  stopTrack(now, LedMode::Off);
  Nfc.stop();
  state_ = PlayerState::Fault;
}

void Player::requestSleep(uint32_t now) {
  Power.disableKeepAlive();
  stopTrack(now, LedMode::Off);
  Nfc.stop();
  state_ = PlayerState::WaitRelease;
}

void Player::sleep() {
  state_ = PlayerState::Sleeping;
  Power.sleep();
}

void Player::button(uint32_t now) {
  bool pressed = Encoder.pressed();
  if (pressed != buttonRaw_) { buttonRaw_ = pressed; buttonChanged_ = now; }
  if (pressed) lastActivity_ = now;
  if (now - buttonChanged_ < Config::BUTTON_DEBOUNCE_MS) return;
  if (!buttonArmed_) {
    if (!pressed) { buttonArmed_ = true; buttonStable_ = false; }
    return;
  }
  if (buttonStable_ != pressed) {
    buttonStable_ = pressed;
    if (pressed) pressedAt_ = now;
    else if (state_ == PlayerState::WaitRelease) { sleep(); return; }
  }
  if (buttonStable_ && state_ != PlayerState::WaitRelease &&
      now - pressedAt_ >= Config::BUTTON_HOLD_MS) requestSleep(now);
}

void Player::startTrack(uint32_t now) {
  stopTrack(now, LedMode::Off);
  if (!Storage.exists(path_)) {
    error_ = "Track not found; remove and re-present tag after fixing SD";
    state_ = PlayerState::Idle;
    Leds.mode(LedMode::Standby, now);
    return;
  }
  Motor.start(now);
  lastActivity_ = now;
  state_ = PlayerState::MotorRamp;
}

void Player::tick(uint32_t now) {
  if (state_ == PlayerState::Fault || state_ == PlayerState::Sleeping) return;
  button(now);
  if (state_ == PlayerState::WaitRelease || state_ == PlayerState::Sleeping) return;
  Power.tick(now);
  int8_t delta = Encoder.delta(now);
  if (delta != 0) {
    int target = static_cast<int>(volume_) + delta;
    if (target < Config::MIN_VOLUME) target = Config::MIN_VOLUME;
    if (target > Config::MAX_VOLUME) target = Config::MAX_VOLUME;
    volume_ = static_cast<uint8_t>(target);
    Audio.volume(volume_);
    lastActivity_ = now;
  }

  uint32_t elapsed = now - bootAt_;
  switch (state_) {
    case PlayerState::BootSD:
      if (elapsed >= Config::BOOT_SD_EARLIEST_MS) {
        if (!Storage.begin()) { fail("SD initialization failed", now); return; }
        state_ = PlayerState::BootNfc;
      }
      return;
    case PlayerState::BootNfc:
      if (elapsed >= Config::BOOT_NFC_EARLIEST_MS) {
        if (!Nfc.begin()) { fail("PN532 initialization failed", now); return; }
        state_ = PlayerState::BootReady;
      }
      return;
    case PlayerState::BootReady:
      if (elapsed >= Config::BOOT_READY_EARLIEST_MS) {
        if (!Audio.begin()) { fail("Audio initialization failed", now); return; }
        Leds.mode(LedMode::Standby, now);
        state_ = PlayerState::Idle;
        lastActivity_ = now;
      }
      return;
    default: break;
  }

  TagState tag;
  if (Nfc.poll(tag)) {
    tag.uid[sizeof(tag.uid) - 1] = '\0';
    lastActivity_ = now;
    if (strcmp(uid_, tag.uid) != 0) {
      memcpy(uid_, tag.uid, sizeof(uid_));
      error_ = "";
      if (uid_[0] == '\0') {
        stopTrack(now, LedMode::Stopping);
        state_ = PlayerState::Idle;
      } else {
        snprintf(path_, sizeof(path_), "%s/%s.mp3", Config::TRACK_DIRECTORY, uid_);
        plays_ = 0;
        Leds.selectPattern();
        startTrack(now);
      }
    }
  }

  if (state_ == PlayerState::MotorRamp) {
    lastActivity_ = now;
    Motor.tick(now);
    if (Motor.ready()) {
      if (!Audio.play(path_, volume_)) {
        stopTrack(now, LedMode::Standby);
        error_ = "MP3 start failed; remove and re-present tag to retry";
        state_ = PlayerState::Idle;
      } else {
        Leds.mode(LedMode::Starting, now);
        state_ = PlayerState::Playing;
      }
    }
  } else if (state_ == PlayerState::Playing) {
    lastActivity_ = now;
    if (!Audio.tick(now)) {
      if (++plays_ < Config::MAX_PLAYS) startTrack(now);
      else { stopTrack(now, LedMode::Standby); state_ = PlayerState::Idle; }
    }
  }
  Leds.tick(now);
  if (state_ == PlayerState::Idle && !buttonRaw_ && !buttonStable_ &&
      now - lastActivity_ >= Config::IDLE_SLEEP_MS) {
    requestSleep(now);
    sleep();
  }
}