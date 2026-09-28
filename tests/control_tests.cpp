#include <cstdio>
#include <cstring>
#include "config.h"
#include "pins.h"
#include "motor.h"
#include "player.h"

int checks = 0;
void require(bool condition, const char *message) {
  ++checks;
  if (!condition) { std::printf("FAIL: %s\n", message); throw 1; }
}
struct FakePower : IPower {
  bool ok = true, disabled = false, slept = false;
  uint32_t ticks = 0;
  bool begin() override { return ok; }
  void tick(uint32_t) override { ++ticks; }
  void disableKeepAlive() override { disabled = true; }
  void sleep() override { slept = true; }
};
struct FakePwm : IPwm {
  bool ok = true;
  uint8_t duty = 0;
  bool begin() override { return ok; }
  void write(uint8_t value) override { duty = value; }
};
struct FakeLeds : ILeds {
  LedMode current = LedMode::Off;
  bool initialized = false;
  int patterns = 0;
  void begin() override { initialized = true; }
  void mode(LedMode value, uint32_t) override { current = value; }
  void selectPattern() override { ++patterns; }
  void tick(uint32_t) override {}
};
struct FakeAudio : IAudio {
  bool ok = true, playOK = true, active = false, continues = true;
  int initialized = 0, starts = 0;
  uint8_t level = 0;
  bool begin() override { ++initialized; return ok; }
  bool play(const char *, uint8_t value) override { level = value; ++starts; active = playOK; return playOK; }
  bool tick(uint32_t) override { return continues; }
  void volume(uint8_t value) override { level = value; }
  void stop() override { active = false; }
};
struct FakeNfc : INfc {
  bool ok = true, pending = false, stopped = false;
  int initialized = 0;
  TagState latest;
  bool begin() override { ++initialized; return ok; }
  bool poll(TagState &tag) override {
    if (!pending) return false;
    tag = latest; pending = false; return true;
  }
  void stop() override { stopped = true; }
  void put(const char *uid) { std::strncpy(latest.uid, uid, sizeof(latest.uid) - 1); pending = true; }
};
struct FakeStorage : IStorage {
  bool ok = true, found = true;
  int initialized = 0, lookups = 0;
  char path[Config::TRACK_PATH_CAPACITY] = {};
  bool begin() override { ++initialized; return ok; }
  bool exists(const char *value) override {
    ++lookups; std::strncpy(path, value, sizeof(path) - 1); return found;
  }
};
struct FakeEncoder : IEncoder {
  bool held = false;
  int8_t turn = 0;
  void begin() override {}
  bool pressed() const override { return held; }
  int8_t delta(uint32_t) override { int8_t value = turn; turn = 0; return value; }
};
struct Rig {
  FakePower power;
  FakePwm pwm;
  MotorControl motor{pwm};
  FakeLeds leds;
  FakeAudio audio;
  FakeNfc nfc;
  FakeStorage storage;
  FakeEncoder encoder;
  Player player{power, motor, leds, audio, nfc, storage, encoder};
  uint32_t now = 0;
  void step(uint32_t increment = 1) { now += increment; player.tick(now); }
  void boot(uint32_t start = 0) {
    now = start; player.begin(now);
    step(Config::BOOT_SD_EARLIEST_MS);
    step(Config::BOOT_NFC_EARLIEST_MS - Config::BOOT_SD_EARLIEST_MS);
    step(Config::BOOT_READY_EARLIEST_MS - Config::BOOT_NFC_EARLIEST_MS);
  }
  void tag(const char *value) { nfc.put(value); step(); }
};

int main() {
  try {
    {
      Rig rig;
      rig.player.begin(0);
      rig.step(Config::BOOT_SD_EARLIEST_MS - 1);
      require(rig.storage.initialized == 0 && rig.pwm.duty == 0, "safe before first boot stage");
      rig.step();
      require(rig.storage.initialized == 1 && rig.nfc.initialized == 0, "SD first");
      rig.step(Config::BOOT_NFC_EARLIEST_MS - Config::BOOT_SD_EARLIEST_MS);
      require(rig.nfc.initialized == 1 && rig.audio.initialized == 0, "NFC second");
      rig.step(Config::BOOT_READY_EARLIEST_MS - Config::BOOT_NFC_EARLIEST_MS);
      require(rig.player.state() == PlayerState::Idle && rig.audio.starts == 0, "ready without unsolicited playback");
      rig.tag("A1");
      require(rig.player.state() == PlayerState::MotorRamp && rig.leds.current == LedMode::Off, "tag starts motor alone");
      uint8_t previous = rig.pwm.duty;
      for (uint32_t elapsed = Config::MOTOR_UPDATE_MS; elapsed < Config::MOTOR_RAMP_MS; elapsed += Config::MOTOR_UPDATE_MS) {
        rig.step(Config::MOTOR_UPDATE_MS);
        require(rig.pwm.duty >= previous && rig.pwm.duty <= Config::MOTOR_RUN_DUTY, "bounded increasing ramp");
        require(rig.audio.starts == 0, "no audio during motor ramp");
        previous = rig.pwm.duty;
      }
      rig.step(Config::MOTOR_UPDATE_MS);
      require(rig.audio.starts == 1 && rig.player.state() == PlayerState::Playing, "audio follows motor");
      require(rig.leds.current == LedMode::Starting && rig.pwm.duty == Config::MOTOR_RUN_DUTY, "LED startup and running duty");
      for (int index = 0; index < 50; ++index) { rig.encoder.turn = 1; rig.step(); }
      require(rig.player.volume() == Config::MAX_VOLUME && rig.audio.level == Config::MAX_VOLUME, "volume upper bound");
      for (int index = 0; index < 50; ++index) { rig.encoder.turn = -1; rig.step(); }
      require(rig.player.volume() == Config::MIN_VOLUME, "volume lower bound");
      rig.audio.continues = false;
      for (uint8_t repeat = 0; repeat < Config::MAX_PLAYS; ++repeat) {
        rig.step();
        if (repeat + 1 < Config::MAX_PLAYS) rig.step(Config::MOTOR_RAMP_MS);
      }
      require(rig.audio.starts == Config::MAX_PLAYS && rig.player.completedPlays() == Config::MAX_PLAYS, "three plays");
      require(rig.player.state() == PlayerState::Idle && rig.pwm.duty == 0, "stop after repeats");
      rig.step(Config::IDLE_SLEEP_MS);
      require(rig.power.slept && rig.power.disabled && rig.nfc.stopped, "idle sleep stops keepalive and NFC");
    }
    {
      Rig rig; rig.boot(); rig.tag("A1"); rig.step(100); rig.tag("");
      rig.step(Config::MOTOR_RAMP_MS);
      require(rig.audio.starts == 0 && rig.pwm.duty == 0, "tag removal cancels ramp");
      rig.tag("A1"); rig.step(100); rig.tag("B2");
      require(std::strcmp(rig.storage.path, "/player/B2.mp3") == 0, "new tag replaces track");
      rig.step(Config::MOTOR_RAMP_MS);
      require(rig.audio.starts == 1, "only latest track plays");
    }
    {
      Rig rig; rig.boot(); rig.storage.found = false; rig.tag("A1");
      for (int iteration = 0; iteration < 10; ++iteration) rig.step();
      require(rig.storage.lookups == 1 && rig.pwm.duty == 0, "missing file does not retry endlessly");
      rig.tag(""); rig.tag("A1");
      require(rig.storage.lookups == 2, "new presentation retries");
    }
    {
      Rig rig; rig.boot(); rig.audio.playOK = false; rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      require(rig.player.state() == PlayerState::Idle && rig.pwm.duty == 0 && !rig.audio.active, "audio failure stops motor");
    }
    {
      Rig rig; rig.encoder.held = true; rig.boot(); rig.step(Config::BUTTON_HOLD_MS + 1);
      require(!rig.power.disabled, "wake button held does not shut down again");
      rig.encoder.held = false; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS);
      rig.tag("A1");
      rig.encoder.held = true; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS - 1);
      require(!rig.power.disabled, "press debounced");
      rig.step(); rig.step(Config::BUTTON_HOLD_MS);
      require(rig.player.state() == PlayerState::WaitRelease && rig.pwm.duty == 0 && !rig.audio.active, "long hold stops loads");
      require(rig.power.disabled && !rig.power.slept, "wait for release without KEY pulses");
      rig.tag("B2"); require(rig.pwm.duty == 0, "no new playback during shutdown");
      rig.encoder.held = false; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS);
      require(rig.power.slept, "release enters sleep");
    }
    {
      Rig rig; rig.boot(UINT32_MAX - 300); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      require(rig.player.state() == PlayerState::Playing, "boot and ramp survive rollover");
    }
    for (int failure = 0; failure < 5; ++failure) {
      Rig rig;
      bool *flags[] = {&rig.power.ok, &rig.pwm.ok, &rig.storage.ok, &rig.nfc.ok, &rig.audio.ok};
      *flags[failure] = false;
      rig.boot(); rig.tag("A1");
      require(rig.player.state() == PlayerState::Fault && rig.pwm.duty == 0 && !rig.audio.active, "initialization fault is fail-closed");
      require(rig.power.disabled, "initialization fault stops KEY pulses");
      require(rig.leds.initialized && rig.leds.current == LedMode::Off, "ring cleared even when power timer fails");
    }
    std::printf("PASS: %d HAL/control assertions\n", checks);
    return 0;
  } catch (...) { return 1; }
}