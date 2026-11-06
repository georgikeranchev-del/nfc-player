#include <cstdio>
#include <cstring>
#include "config.h"
#include "pins.h"
#include "motor.h"
#include "player.h"
#include "power.h"

int checks = 0;
void require(bool condition, const char *message) {
  ++checks;
  if (!condition) { std::printf("FAIL: %s\n", message); throw 1; }
}
struct FakePower : IPower {
  bool ok = true, slept = false, buttonWake = true, chargerWake = false, charger = false;
  bool inhibited = false;
  uint32_t ticks = 0;
  int rails = 0;
  uint32_t millivolts = 3800;
  bool begin() override { return ok; }
  void enablePeripherals() override { ++rails; }
  void tick(uint32_t) override { ++ticks; }
  uint32_t batteryMillivolts() const override { return millivolts; }
  bool playbackInhibited() const override { return inhibited; }
  void inhibitPlayback(bool value) override { inhibited = value; }
  bool chargerPresent() const override { return charger; }
  void sleep(bool allowButtonWake, bool allowChargerWake) override {
    slept = true; buttonWake = allowButtonWake; chargerWake = allowChargerWake;
  }
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
  char path[Config::TRACK_PATH_CAPACITY] = {};
  bool begin() override { ++initialized; return ok; }
  bool play(const char *value, uint8_t volume) override {
    std::strncpy(path, value, sizeof(path) - 1);
    level = volume; ++starts; active = playOK; return playOK;
  }
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
  bool held = false, help = false;
  int8_t turn = 0;
  void begin() override {}
  bool pressed() const override { return held; }
  bool helpPressed() const override { return help; }
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
  void press(bool down) { encoder.help = down; step(); step(Config::BUTTON_DEBOUNCE_MS); }
};

int main() {
  try {
    require(PowerControl::batteryMillivoltsFromAdc(0) == 0, "zero ADC stays zero");
    require(PowerControl::batteryMillivoltsFromAdc(1343, 1000) == 4200, "full 1S pack divider conversion");
    require(PowerControl::batteryMillivoltsFromAdc(1151, 1000) == 3600, "nominal 1S pack divider conversion");
    require(PowerControl::batteryMillivoltsFromAdc(1000, 1000) == 3128, "divider conversion rounds millivolts");
    require(PowerControl::batteryMillivoltsFromAdc(1343, 1020) == 4284, "meter calibration scales pack reading");
    {
      Rig rig;
      rig.player.begin(0);
      rig.step(Config::BOOT_SD_EARLIEST_MS - 1);
      require(rig.storage.initialized == 0 && rig.pwm.duty == 0, "safe before first boot stage");
      require(rig.power.rails == 1, "peripheral rail enabled once, before the SD stage");
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
      require(rig.power.slept && rig.power.buttonWake && !rig.power.chargerWake && rig.nfc.stopped,
        "idle sleep allows only button wake and stops NFC");
    }
          {
            Rig rig; rig.boot();
            uint32_t powerTicks = rig.power.ticks;
            rig.step(Config::IDLE_SLEEP_MS - 1);
            require(rig.player.state() == PlayerState::Idle && rig.pwm.duty == 0 && rig.audio.starts == 0,
              "no-tag idle keeps motor and audio off until the timeout");
            require(rig.leds.current == LedMode::Standby && !rig.nfc.stopped,
              "no-tag idle retains standby LEDs and NFC polling");
            require(rig.power.ticks > powerTicks && !rig.power.slept,
              "no-tag idle continues monitoring battery");
            rig.step();
            require(rig.player.state() == PlayerState::Sleeping && rig.power.slept && rig.power.buttonWake && rig.nfc.stopped,
              "no-tag timeout sleeps with button wake and stops NFC");
            require(rig.pwm.duty == 0 && !rig.audio.active && rig.leds.current == LedMode::Off,
              "no-tag sleep leaves motor audio and LEDs off");
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
      require(rig.player.state() == PlayerState::Idle, "wake button held does not shut down again");
      rig.encoder.held = false; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS);
      rig.tag("A1");
      rig.encoder.held = true; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS - 1);
      require(rig.player.state() == PlayerState::MotorRamp, "press debounced");
      rig.step(); rig.step(Config::BUTTON_HOLD_MS);
      require(rig.player.state() == PlayerState::WaitRelease && rig.pwm.duty == 0 && !rig.audio.active, "long hold stops loads");
      require(rig.nfc.stopped && !rig.power.slept, "wait for release with NFC stopped");
      rig.tag("B2"); require(rig.pwm.duty == 0, "no new playback during shutdown");
      rig.encoder.held = false; rig.step(); rig.step(Config::BUTTON_DEBOUNCE_MS);
      require(rig.power.slept && rig.power.buttonWake, "release enters sleep with button wake");
    }
    {
      Rig rig; rig.boot(UINT32_MAX - 300); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      require(rig.player.state() == PlayerState::Playing, "boot and ramp survive rollover");
    }
    {
      const uint32_t unsafeVoltages[] = {0, Config::BATTERY_CRITICAL_MV, Config::BATTERY_LOW_MV,
        Config::BATTERY_MAX_VALID_MV + 1};
      for (uint32_t voltage : unsafeVoltages) {
        Rig rig; rig.power.millivolts = voltage; rig.encoder.held = true; rig.boot();
        require(rig.player.state() == PlayerState::Sleeping && rig.power.slept && !rig.power.buttonWake,
          "unsafe startup voltage sleeps without button wake even if held");
        require(rig.power.chargerWake, "unsafe startup voltage can wake for a charger");
        require(rig.storage.initialized == 0 && rig.nfc.initialized == 0 && rig.audio.initialized == 0,
          "unsafe startup voltage blocks peripheral initialization");
        require(rig.power.rails == 0, "unsafe startup never switches the peripheral rail on");
        require(rig.pwm.duty == 0 && !rig.audio.active && rig.leds.current == LedMode::Off,
          "unsafe startup leaves loads off");
      }
    }
    {
      Rig rig; rig.power.millivolts = Config::BATTERY_START_MIN_MV; rig.boot();
      require(rig.player.state() == PlayerState::Idle, "startup threshold is inclusive");
      rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      rig.power.millivolts = Config::BATTERY_LOW_MV; rig.step();
      rig.step(Config::BATTERY_LOW_HOLD_MS - 1);
      require(!rig.power.slept && rig.audio.active, "short low-voltage dip does not stop playback");
      rig.power.millivolts = Config::BATTERY_LOW_MV + 1; rig.step();
      rig.power.millivolts = Config::BATTERY_LOW_MV; rig.step();
      rig.step(Config::BATTERY_LOW_HOLD_MS - 1);
      require(!rig.power.slept, "voltage recovery resets low-voltage timer");
      rig.step();
      require(rig.player.state() == PlayerState::Sleeping && rig.power.slept && !rig.power.buttonWake,
        "sustained low voltage sleeps without button wake");
      require(rig.pwm.duty == 0 && !rig.audio.active && rig.leds.current == LedMode::Off && rig.nfc.stopped,
        "low voltage stops motor audio LEDs and NFC");
      rig.power.millivolts = 4200; rig.tag("B2");
      require(rig.player.state() == PlayerState::Sleeping && rig.audio.starts == 1,
        "voltage rebound cannot restart playback");
    }
    {
      Rig rig; rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.boot();
      require(rig.player.state() == PlayerState::Idle && rig.power.inhibited && !rig.nfc.stopped,
        "soft-low startup keeps NFC available but inhibits songs");
      require(rig.leds.current == LedMode::Off && rig.audio.starts == 0, "soft-low startup is quiet and dark");
      rig.tag("A1");
      require(rig.player.state() == PlayerState::BatteryWarning && rig.pwm.duty == 0,
        "low-battery presentation plays warning without motor");
      require(std::strcmp(rig.audio.path, "/player/low_battery.mp3") == 0 &&
        rig.audio.level == Config::LOW_BATTERY_VOLUME, "dedicated MP3 and bounded warning volume");
      for (int turn = 0; turn < 30; ++turn) { rig.encoder.turn = 1; rig.step(); }
      require(rig.audio.level == Config::LOW_BATTERY_VOLUME, "encoder cannot increase warning volume");
      rig.tag("");
      require(rig.player.state() == PlayerState::BatteryWarning && rig.audio.active,
        "removing the tag does not truncate warning");
      rig.audio.continues = false; rig.step();
      require(rig.player.state() == PlayerState::Idle && rig.audio.starts == 1 && rig.player.completedPlays() == 0,
        "warning finishes once without song repeats");
      rig.audio.continues = true; rig.tag("A1");
      require(rig.audio.starts == 2, "re-presenting same tag produces one more warning");
      rig.step(Config::LOW_BATTERY_MAX_MS);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active && rig.audio.starts == 2,
        "warning has a maximum duration and cannot loop");
      rig.step();
      require(rig.audio.starts == 2, "held tag does not repeatedly trigger warning");
    }
    {
      Rig rig; rig.boot(); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.step();
      rig.step(Config::BATTERY_SOFT_HOLD_MS - 1);
      require(rig.audio.active && !rig.power.inhibited, "short soft-low dip does not interrupt current song");
      rig.power.millivolts = Config::BATTERY_START_MIN_MV; rig.step();
      rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.step();
      rig.step(Config::BATTERY_SOFT_HOLD_MS);
      require(rig.power.inhibited && !rig.power.slept && rig.pwm.duty == 0 && !rig.audio.active && !rig.nfc.stopped,
        "sustained soft-low voltage stops song but keeps NFC awake");
      rig.power.millivolts = 3700; rig.step(); rig.tag("B2");
      require(rig.player.state() == PlayerState::BatteryWarning && rig.pwm.duty == 0,
        "immediate unloaded voltage rebound cannot clear song inhibition");
      rig.power.millivolts = Config::BATTERY_CRITICAL_MV; rig.step();
      require(rig.power.slept && !rig.audio.active && !rig.power.buttonWake,
        "critical voltage preempts the warning MP3");
    }
    {
      Rig rig; rig.boot(); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.audio.continues = false; rig.step();
      require(rig.power.inhibited && rig.player.state() == PlayerState::Idle && rig.audio.starts == 1,
        "automatic song repeat cannot bypass inhibition or trigger warning loop");
    }
    {
      Rig rig; rig.power.inhibited = true; rig.boot();
      require(rig.power.inhibited && !rig.power.slept, "soft inhibition survives normal sleep wake despite voltage rebound");
      rig.storage.found = false; rig.tag("A1");
      require(rig.player.state() == PlayerState::Idle && rig.pwm.duty == 0 && rig.audio.starts == 0,
        "missing warning MP3 never falls through to song playback");
      require(std::strstr(rig.player.error(), "missing") != nullptr, "missing warning is reported");
      rig.storage.found = true; rig.audio.playOK = false; rig.tag("B2");
      require(rig.player.state() == PlayerState::Idle && rig.pwm.duty == 0 && !rig.audio.active,
        "failed warning MP3 remains fail-closed");
    }
    {
      Rig rig; rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.boot();
      rig.power.millivolts = Config::BATTERY_RECOVER_MV; rig.step(Config::BATTERY_RECOVER_HOLD_MS);
      require(rig.power.inhibited, "voltage rebound without a charger cannot recover");
      rig.power.charger = true; rig.step(); rig.step(Config::BATTERY_RECOVER_HOLD_MS - 1);
      require(rig.power.inhibited, "recovery waits for the full hold time");
      rig.power.millivolts = Config::BATTERY_RECOVER_MV - 1; rig.step();
      rig.power.millivolts = Config::BATTERY_RECOVER_MV; rig.step(); rig.step(Config::BATTERY_RECOVER_HOLD_MS - 1);
      require(rig.power.inhibited, "interrupted charger hold restarts");
      rig.step();
      require(!rig.power.inhibited && rig.leds.current == LedMode::Standby && !rig.power.slept,
        "charger plus sustained voltage clears song inhibition");
      rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      require(rig.audio.starts == 1 && std::strcmp(rig.audio.path, "/player/A1.mp3") == 0,
        "songs play again after recovery");
    }
    {
      Rig rig; rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.power.charger = true; rig.boot();
      rig.step(Config::BATTERY_RECOVER_HOLD_MS);
      require(rig.power.inhibited, "charger alone without voltage recovery stays inhibited");
      rig.power.millivolts = Config::BATTERY_RECOVER_MV; rig.step(); rig.power.charger = false;
      rig.step(Config::BATTERY_RECOVER_HOLD_MS);
      require(rig.power.inhibited, "unplugging the charger cancels recovery");
    }
    {
      Rig rig; rig.boot(UINT32_MAX - 1000);
      rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.step(); rig.step(Config::BATTERY_SOFT_HOLD_MS);
      require(rig.power.inhibited && !rig.power.slept, "soft-inhibit timer survives rollover");
      require(rig.leds.current == LedMode::Off, "soft inhibition also darkens an idle player");
    }
    {
      const uint32_t unsafeVoltages[] = {0, Config::BATTERY_CRITICAL_MV, Config::BATTERY_MAX_VALID_MV + 1};
      for (uint32_t voltage : unsafeVoltages) {
        Rig rig; rig.boot(); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
        rig.encoder.held = true; rig.power.millivolts = voltage; rig.step();
        require(rig.power.slept && !rig.power.buttonWake && rig.pwm.duty == 0 && !rig.audio.active,
          "critical or invalid voltage stops immediately without waiting for button release");
      }
    }
    {
      Rig rig; rig.power.charger = true; rig.boot(); rig.tag("A1"); rig.step(Config::MOTOR_RAMP_MS);
      require(rig.player.state() == PlayerState::Playing, "charging reaches playback");
      rig.power.millivolts = Config::BATTERY_MAX_VALID_MV + 1; rig.step(); rig.step(Config::BATTERY_LOW_HOLD_MS);
      require(!rig.power.slept && rig.audio.active && rig.player.state() == PlayerState::Playing,
        "over-range reading while charging does not cut playback");
    }
    {
      Rig rig; rig.boot(); rig.tag("A1");
      rig.power.millivolts = Config::BATTERY_CRITICAL_MV; rig.step();
      require(rig.power.slept && rig.pwm.duty == 0 && rig.audio.starts == 0,
        "critical voltage during motor ramp prevents audio startup");
    }
    {
      Rig rig; rig.boot(UINT32_MAX - 1000);
      rig.power.millivolts = Config::BATTERY_LOW_MV; rig.step(); rig.step(Config::BATTERY_LOW_HOLD_MS);
      require(rig.power.slept && !rig.power.buttonWake, "low-voltage timer survives rollover");
    }
    {
      Rig rig; rig.boot(); rig.press(true);
      require(rig.player.state() == PlayerState::Help && rig.audio.starts == 1 && rig.pwm.duty == 0,
        "help press plays help without the motor");
      require(std::strcmp(rig.audio.path, "/player/help.mp3") == 0 && rig.audio.level == rig.player.volume(),
        "help uses help.mp3 at the selected volume");
      rig.encoder.turn = 1; rig.step();
      require(rig.audio.level == rig.player.volume(), "encoder adjusts help volume");
      rig.press(false); rig.press(true);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active && rig.leds.current == LedMode::Standby,
        "second press stops help");
      rig.press(false); rig.press(true);
      rig.audio.continues = false; rig.step();
      require(rig.player.state() == PlayerState::Idle && rig.audio.starts == 2 && rig.leds.current == LedMode::Standby,
        "help ends when the file finishes");
    }
    {
      Rig rig; rig.boot(); rig.press(true);
      rig.step(Config::HELP_MAX_MS);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active, "help has a maximum duration");
    }
    {
      Rig rig; rig.boot(); rig.press(true);
      rig.tag("A1");
      require(rig.player.state() == PlayerState::MotorRamp && !rig.audio.active, "a tag interrupts help");
      rig.step(Config::MOTOR_RAMP_MS);
      require(rig.player.state() == PlayerState::Playing && std::strcmp(rig.audio.path, "/player/A1.mp3") == 0,
        "song plays after help interruption");
      rig.press(false); rig.press(true);
      require(rig.player.state() == PlayerState::Playing && rig.audio.starts == 2, "help is ignored during a song");
    }
    {
      Rig rig; rig.boot(); rig.tag("A1"); rig.tag(""); rig.press(true);
      rig.tag("");
      require(rig.player.state() == PlayerState::Help, "tag removal does not stop help");
    }
    {
      Rig rig; rig.encoder.help = true; rig.boot(); rig.step(Config::BUTTON_DEBOUNCE_MS);
      require(rig.player.state() == PlayerState::Idle && rig.audio.starts == 0, "button held at boot does not play help");
      rig.press(false); rig.press(true);
      require(rig.player.state() == PlayerState::Help, "help works after the boot-time press is released");
    }
    {
      Rig rig; rig.boot(); rig.encoder.help = true; rig.step(); rig.encoder.help = false; rig.step(Config::BUTTON_DEBOUNCE_MS);
      require(rig.audio.starts == 0 && rig.player.state() == PlayerState::Idle, "contact bounce shorter than debounce is ignored");
      rig.storage.found = false; rig.press(true);
      require(rig.player.state() == PlayerState::Idle && rig.audio.starts == 0 &&
        std::strstr(rig.player.error(), "Help") != nullptr, "missing help file is reported and fails closed");
      rig.storage.found = true; rig.audio.playOK = false; rig.press(false); rig.press(true);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active, "failed help audio fails closed");
    }
    {
      Rig rig; rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.boot(); rig.press(true);
      require(rig.player.state() == PlayerState::BatteryWarning &&
        std::strcmp(rig.audio.path, "/player/low_battery.mp3") == 0, "low battery plays the warning instead of help");
    }
    {
      Rig rig; rig.boot(); rig.press(true);
      rig.power.millivolts = Config::BATTERY_START_MIN_MV - 1; rig.step(); rig.step(Config::BATTERY_SOFT_HOLD_MS);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active && rig.power.inhibited,
        "sustained low voltage stops help");
    }
    {
      Rig rig; rig.boot(UINT32_MAX - 300); rig.press(true);
      rig.step(Config::HELP_MAX_MS);
      require(rig.player.state() == PlayerState::Idle && !rig.audio.active, "help timer survives rollover");
    }
    for (int failure = 0; failure < 5; ++failure) {
      Rig rig;
      bool *flags[] = {&rig.power.ok, &rig.pwm.ok, &rig.storage.ok, &rig.nfc.ok, &rig.audio.ok};
      *flags[failure] = false;
      rig.boot(); rig.tag("A1");
      require(rig.player.state() == PlayerState::Fault && rig.pwm.duty == 0 && !rig.audio.active, "initialization fault is fail-closed");
      require(rig.nfc.stopped, "initialization fault stops NFC");
      require(rig.leds.initialized && rig.leds.current == LedMode::Off, "ring cleared even when power monitoring fails");
      rig.power.millivolts = Config::BATTERY_CRITICAL_MV; rig.step();
      require(rig.power.slept && !rig.power.buttonWake, "battery monitoring continues after initialization faults");
    }
    std::printf("PASS: %d HAL/control assertions\n", checks);
    return 0;
  } catch (...) { return 1; }
}