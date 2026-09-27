#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

using std::min;
using std::max;

constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT_PULLUP = 2;
constexpr int HSPI = 1, WS2812B = 1, GRB = 1;
constexpr int ESP_OK = 0, pdTRUE = 1, pdPASS = 1;
using gpio_num_t = int;
uint32_t fakeNow = 0;
int pins[40] = {};
int motorDuty = 0;
int pwmPinOrChannel = -1;
bool pwmOK = true, sdOK = true, fileExists = true, audioOK = true;
bool pnOK = true, samOK = true, timerOK = true, armOK = true;
bool queueOK = true, taskOK = true, audioContinues = true;
int existsCalls = 0, audioBegins = 0, publications = 0;
struct SleepEntered {};
struct TaskFinished {};

struct FakeSerial {
  void begin(int) {}
  void flush() {}
  template<class Value> void print(Value) {}
  template<class Value> void println(Value) {}
  template<class... Values> void printf(const char *, Values...) {}
} Serial;

struct FakeTimer {
  bool active = false;
  uint32_t start = 0, duration = 0;
  void (*callback)(void *) = nullptr;
} timerStorage;
using esp_timer_handle_t = FakeTimer *;
struct esp_timer_create_args_t {
  void (*callback)(void *) = nullptr;
  const char *name = nullptr;
};
int esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *handle) {
  if (!timerOK) return -1;
  timerStorage.callback = args->callback;
  *handle = &timerStorage;
  return ESP_OK;
}
int esp_timer_start_once(esp_timer_handle_t timer, uint64_t micros) {
  if (!armOK) return -1;
  timer->active = true;
  timer->start = fakeNow;
  timer->duration = static_cast<uint32_t>(micros / 1000);
  return ESP_OK;
}
int esp_timer_stop(esp_timer_handle_t timer) { timer->active = false; return ESP_OK; }
bool esp_timer_is_active(esp_timer_handle_t timer) { return timer->active; }
void advance(uint32_t milliseconds) {
  fakeNow += milliseconds;
  if (timerStorage.active && fakeNow - timerStorage.start >= timerStorage.duration) {
    timerStorage.active = false;
    timerStorage.callback(nullptr);
  }
}
uint32_t millis() { return fakeNow; }
void delay(uint32_t milliseconds) { advance(milliseconds); }
void digitalWrite(int pin, int value) { pins[pin] = value; }
int digitalRead(int pin) { return pins[pin]; }
void pinMode(int, int) {}
void ledcWrite(int pinOrChannel, int duty) { pwmPinOrChannel = pinOrChannel; motorDuty = duty; }
bool ledcAttach(int, uint32_t, uint8_t) { return pwmOK; }
double ledcSetup(int, uint32_t, uint8_t) { return pwmOK ? 20000.0 : 0.0; }
void ledcAttachPin(int, int) {}
void rtc_gpio_deinit(int) {}
void rtc_gpio_pullup_en(int) {}
void rtc_gpio_pulldown_dis(int) {}
void esp_sleep_enable_ext0_wakeup(int, int) {}
void esp_deep_sleep_start() { throw SleepEntered{}; }
int esp_reset_reason() { return 1; }
int esp_sleep_get_wakeup_cause() { return 0; }
uint32_t esp_random() { static uint32_t seed = 0; return ++seed; }
void randomSeed(uint32_t) {}
void random16_set_seed(uint32_t) {}
uint8_t random8() { return 100; }
uint8_t random8(uint8_t) { return 0; }
uint8_t random8(uint8_t low, uint8_t) { return low; }
uint8_t beatsin8(uint8_t, uint8_t low, uint8_t) { return low; }
uint8_t sin8(uint16_t) { return 0; }
uint8_t scale8(uint8_t value, uint8_t scale) { return value * scale / 255; }
long map(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
  return (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}
struct CRGB {
  CRGB() {}
  CRGB(int, int, int) {}
  CRGB &operator+=(const CRGB &) { return *this; }
  void nscale8(uint8_t) {}
  static const CRGB Black, White;
};
const CRGB CRGB::Black{}, CRGB::White{};
void fill_solid(CRGB *, int, const CRGB &) {}
void fadeToBlackBy(CRGB *, int, uint8_t) {}
struct FakeLED {
  template<int Type, int Pin, int Order> void addLeds(CRGB *, int) {}
  void setBrightness(uint8_t) {}
  void clear() {}
  void show() {}
} FastLED;
struct SPIClass {
  SPIClass(int) {}
  void begin(int, int, int, int) {}
};
struct FakeSD {
  bool begin(int, SPIClass &, uint32_t) { return sdOK; }
  bool exists(const char *) { ++existsCalls; return fileExists; }
  void mkdir(const char *) {}
} SD;
struct AudioFileSourceSD {
  AudioFileSourceSD(const char *) {}
  void close() {}
};
struct AudioFileSourceBuffer {
  AudioFileSourceBuffer(AudioFileSourceSD *, size_t) {}
};
struct AudioOutputI2S {
  float gain = 0;
  void SetPinout(int, int, int) {}
  void SetGain(float value) { gain = value; }
};
struct AudioGeneratorMP3 {
  bool running = false;
  bool isRunning() { return running; }
  void stop() { running = false; }
  bool begin(AudioFileSourceBuffer *, AudioOutputI2S *) {
    ++audioBegins;
    running = audioOK;
    return audioOK;
  }
  bool loop() { return audioContinues; }
};
struct FakeQueue {
  char uid[15] = {};
  bool pending = false;
} queueStorage;
using QueueHandle_t = FakeQueue *;
QueueHandle_t xQueueCreate(int, size_t) { return queueOK ? &queueStorage : nullptr; }
int xQueueOverwrite(QueueHandle_t queue, const void *presence) {
  std::memcpy(queue->uid, presence, sizeof(queue->uid));
  queue->pending = true;
  ++publications;
  return pdTRUE;
}
int xQueueReceive(QueueHandle_t queue, void *presence, int) {
  if (!queue->pending) return 0;
  std::memcpy(presence, queue->uid, sizeof(queue->uid));
  queue->pending = false;
  return pdTRUE;
}
int xTaskCreatePinnedToCore(void (*)(void *), const char *, int, void *, int, void *, int) {
  return taskOK ? pdPASS : 0;
}
uint32_t pdMS_TO_TICKS(uint32_t ticks) { return ticks; }
constexpr int PN532_MIFARE_ISO14443A = 1;
std::vector<int> nfcReads;
size_t nfcReadIndex = 0;
void vTaskDelay(uint32_t ticks) {
  advance(ticks);
  if (nfcReadIndex >= nfcReads.size()) throw TaskFinished{};
}
struct Adafruit_PN532 {
  Adafruit_PN532(int, int, int, int) {}
  void begin() {}
  uint32_t getFirmwareVersion() { return pnOK ? 1 : 0; }
  bool SAMConfig() { return samOK; }
  void setPassiveActivationRetries(int) {}
  bool readPassiveTargetID(int, uint8_t *uid, uint8_t *length, int) {
    if (nfcReadIndex >= nfcReads.size()) throw TaskFinished{};
    int value = nfcReads[nfcReadIndex++];
    if (value < 0) return false;
    *length = 1;
    *uid = static_cast<uint8_t>(value);
    return true;
  }
};

#include "nfc_player_under_test.inc"

int checks = 0;
void require(bool condition, const char *message) {
  ++checks;
  if (!condition) { std::printf("FAIL: %s\n", message); throw 1; }
}
void resetTest(uint32_t now = 0) {
  stopPlayback();
  fakeNow = now;
  std::fill(pins, pins + 40, HIGH);
  forceOutputsSafe();
  pwmOK = sdOK = fileExists = audioOK = pnOK = samOK = true;
  timerOK = armOK = queueOK = taskOK = audioContinues = true;
  existsCalls = audioBegins = publications = 0;
  motorDuty = 0;
  timerStorage = {};
  timerStorage.callback = releaseKey;
  keyReleaseTimer = &timerStorage;
  queueStorage = {};
  nfcQueue = &queueStorage;
  nfcReads.clear();
  nfcReadIndex = 0;
  systemReady = true;
  shutdownPending = buttonRawPressed = buttonStablePressed = false;
  buttonChangedAt = buttonPressStart = now;
  lastActivity = lastKeepAlive = now;
  lastLEDUpdate = now;
  loopCount = 0;
  volume = INITIAL_VOLUME;
  currentUID[0] = playingFile[0] = '\0';
  if (out == nullptr) out = new AudioOutputI2S();
  out->SetGain(0);
}
void present(const char *uid) {
  NFCPresence presence = {};
  std::strncpy(presence.uid, uid, sizeof(presence.uid) - 1);
  xQueueOverwrite(nfcQueue, &presence);
  loop();
}
void finishRamp() { advance(MOTOR_SOFT_START_MS); updatePlaybackStartup(); }

int main() {
  try {
    resetTest();
    require(setupMotorPWM(), "PWM initializes");
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    require(pwmPinOrChannel == MOTOR_PIN, "3.x PWM uses pin");
#else
    require(pwmPinOrChannel == MOTOR_PWM_CHANNEL, "2.x PWM uses channel");
#endif
    require(playTrack("/player/A1.mp3"), "track startup accepted");
    require(fakeNow == 0 && playbackPending && !isPlaying, "motor start does not wait or start audio");
    int previousDuty = motorDuty;
    for (uint32_t elapsed = MOTOR_RAMP_STEP_MS; elapsed < MOTOR_SOFT_START_MS; elapsed += MOTOR_RAMP_STEP_MS) {
      advance(MOTOR_RAMP_STEP_MS);
      updatePlaybackStartup();
      require(motorDuty >= previousDuty && motorDuty <= MOTOR_RUN_DUTY, "ramp monotonic and bounded");
      require(audioBegins == 0, "audio waits for motor ramp");
      previousDuty = motorDuty;
    }
    advance(MOTOR_RAMP_STEP_MS);
    updatePlaybackStartup();
    require(isPlaying && !playbackPending && audioBegins == 1, "audio starts at ramp completion");
    require(motorDuty == MOTOR_RUN_DUTY && out->gain == 0, "run duty held and fade starts silent");
    advance(AUDIO_FADE_IN_MS / 2);
    volume = 10;
    applyGain();
    require(std::fabs(out->gain - volumeToGain(volume) * 0.5f) < 0.001f, "volume changes honor fade");
    advance(AUDIO_FADE_IN_MS / 2);
    applyGain();
    require(!audioFadingIn && std::fabs(out->gain - volumeToGain(volume)) < 0.001f, "fade completes at target");

    resetTest();
    present("A1");
    advance(100);
    present("");
    advance(MOTOR_SOFT_START_MS);
    loop();
    require(!isPlaying && !playbackPending && motorDuty == 0 && audioBegins == 0, "removal cancels pending startup");
    present("A1");
    advance(100);
    present("B2");
    require(std::strcmp(playingFile, "/player/B2.mp3") == 0, "new tag replaces pending track");
    finishRamp();
    require(audioBegins == 1 && isPlaying, "only replacement track begins");

    resetTest();
    fileExists = false;
    present("A1");
    for (int iteration = 0; iteration < 20; ++iteration) loop();
    require(existsCalls == 1 && !playbackPending && motorDuty == 0, "missing file does not restart repeatedly");
    present("");
    present("A1");
    require(existsCalls == 2, "re-presentation retries missing file");
    resetTest();
    audioOK = false;
    present("A1");
    finishRamp();
    require(!isPlaying && !playbackPending && motorDuty == 0 && mp3 == nullptr, "decoder failure stops loads and frees resources");

    resetTest();
    present("A1");
    finishRamp();
    audioContinues = false;
    for (int repeat = 0; repeat < MAX_LOOPS; ++repeat) {
      loop();
      if (repeat + 1 < MAX_LOOPS) {
        require(playbackPending, "repeat schedules startup");
        finishRamp();
      }
    }
    require(loopCount == MAX_LOOPS && audioBegins == MAX_LOOPS && !isPlaying && !playbackPending, "exactly three plays then idle");

    resetTest();
    isPlaying = true;
    advance(IP5310_KEEPALIVE_INTERVAL);
    updateKeepAlive();
    require(pins[IP5310_KEY] == HIGH, "keep-alive also runs during playback");
    advance(IP5310_KEEPALIVE_PULSE_MS - 1);
    require(pins[IP5310_KEY] == HIGH, "KEY stays active before deadline");
    advance(1);
    require(pins[IP5310_KEY] == LOW, "timer releases KEY without running main loop");
    armOK = false;
    advance(IP5310_KEEPALIVE_INTERVAL);
    updateKeepAlive();
    require(pins[IP5310_KEY] == LOW, "timer arm failure releases KEY");

    resetTest();
    present("A1");
    pins[ENCODER_SW] = LOW;
    handlePowerButton();
    advance(BUTTON_DEBOUNCE_MS - 1);
    handlePowerButton();
    require(!buttonStablePressed, "button press debounced");
    advance(1);
    handlePowerButton();
    advance(LONG_PRESS_MS);
    digitalWrite(IP5310_KEY, HIGH);
    esp_timer_start_once(keyReleaseTimer, 120000);
    handlePowerButton();
    require(shutdownPending && !playbackPending && motorDuty == 0, "hold stops pending motor startup");
    require(pins[IP5310_KEY] == LOW && !timerStorage.active && pins[AMP_SD] == LOW, "shutdown releases KEY and mutes");
    present("B2");
    require(!playbackPending, "pending shutdown blocks new tracks");
    pins[ENCODER_SW] = HIGH;
    handlePowerButton();
    advance(BUTTON_DEBOUNCE_MS);
    bool slept = false;
    try { handlePowerButton(); } catch (SleepEntered &) { slept = true; }
    require(slept, "sleep begins after debounced release");

    resetTest(UINT32_MAX - 100);
    playTrack("/player/A1.mp3");
    finishRamp();
    require(isPlaying && motorDuty == MOTOR_RUN_DUTY, "motor timer survives millis wrap");
    advance(IP5310_KEEPALIVE_INTERVAL);
    updateKeepAlive();
    advance(IP5310_KEEPALIVE_PULSE_MS);
    require(pins[IP5310_KEY] == LOW, "KEY timing survives millis wrap");

    resetTest();
    nfcReads = {0xA1, 0xA1, -1, -1, 0xB2};
    try { nfcTask(nullptr); } catch (TaskFinished &) {}
    require(publications == 3 && std::strcmp(queueStorage.uid, "B2") == 0, "NFC publishes transitions and retains latest state");
    loop();
    require(std::strcmp(currentUID, "B2") == 0 && playbackPending, "latest tag is not followed by stale removal");

    for (int failure = 0; failure < 7; ++failure) {
      resetTest();
      systemReady = false;
      bool *flags[] = {&pwmOK, &sdOK, &pnOK, &samOK, &timerOK, &queueOK, &taskOK};
      *flags[failure] = false;
      setup();
      loop();
      require(!systemReady && motorDuty == 0 && pins[AMP_SD] == LOW && pins[IP5310_KEY] == LOW, "initialization failure stays fail-closed");
    }
    std::printf("PASS: %d checks (Arduino-ESP32 API branch %d)\n", checks, ESP_ARDUINO_VERSION_MAJOR);
    stopPlayback();
    delete out;
    return 0;
  } catch (...) { return 1; }
}