#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_PN532.h>
#include <FastLED.h>
#include "driver/rtc_io.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_arduino_version.h"
#include "freertos/queue.h"
#include <new>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// ============================================================
// PINS
// ============================================================

#define SD_CS       13
#define SD_SCK      14
#define SD_MISO     16
#define SD_MOSI     15

#define PN532_SS    5
#define PN532_SCK   18
#define PN532_MISO  19
#define PN532_MOSI  23

#define MOTOR_PIN   27

#define I2S_LRC     25
#define I2S_BCLK    26
#define I2S_DOUT    22

#define ENCODER_CLK 32
#define ENCODER_DT  33
#define ENCODER_SW  4

#define LED_PIN     21
#define NUM_LEDS    24

#define AMP_SD      2    // SD_MODE controls shutdown/channel selection; GAIN is separate.
#define IP5310_KEY  17   // drives a transistor that taps the IP5310 KEY pin to keep it awake

// ============================================================
// SETTINGS
// ============================================================

const uint8_t MAX_LOOPS = 3;
const uint32_t SD_SPEED = 10000000;
const size_t AUDIO_BUFFER_SIZE = 8192;
const uint32_t AUDIO_FADE_IN_MS = 1000;  // ramp volume 0 -> target so the amp's startup current rises gently

// Periodically tap the IP5310 KEY pin so its low-load auto-shutdown doesn't cut power while we're awake.
const bool IP5310_KEEPALIVE_ENABLED = true;
const uint32_t IP5310_KEEPALIVE_INTERVAL = 20000;  // < ~30 s bank dropout, with margin
const uint16_t IP5310_KEEPALIVE_PULSE_MS = 120;

const uint32_t LONG_PRESS_MS = 1500;
const uint32_t BUTTON_DEBOUNCE_MS = 40;

const uint32_t NFC_SCAN_INTERVAL = 150;
const uint8_t NFC_MISSING_LIMIT = 2;

const uint32_t IDLE_SLEEP_TIMEOUT = 5UL * 60UL * 1000UL;  // deep sleep after 5 min of inactivity

const uint8_t INITIAL_VOLUME = 12;
const uint8_t MIN_VOLUME = 0;
const uint8_t MAX_VOLUME = 21;

// ============================================================
// MOTOR SOFT-START (PWM)
// ============================================================

// PWM is open-loop; the ramp does not impose a measured current limit.
const uint8_t MOTOR_PWM_CHANNEL = 0;
const uint32_t MOTOR_PWM_FREQ = 20000;   // 20 kHz: above hearing range, silent switching
const uint8_t MOTOR_PWM_RES = 8;         // 8-bit duty (0..255)
const uint8_t MOTOR_DUTY_MAX = 255;      // absolute ceiling
const uint8_t MOTOR_RUN_DUTY = 230;      // steady turntable speed -- tune this to hit ~33 1/3 RPM
const uint8_t MOTOR_DUTY_MIN = 60;
const uint32_t MOTOR_SOFT_START_MS = 600; // ramp duration; also the head-start before audio
const uint16_t MOTOR_RAMP_STEP_MS = 15;   // time between ramp steps

static_assert(MOTOR_DUTY_MIN <= MOTOR_RUN_DUTY && MOTOR_RUN_DUTY <= MOTOR_DUTY_MAX,
              "Motor duty must satisfy MIN <= RUN <= MAX");
static_assert(MOTOR_SOFT_START_MS > 0 && AUDIO_FADE_IN_MS > 0,
              "Ramp durations must be positive");

// ============================================================
// LED SETTINGS
// ============================================================

const uint8_t LED_BRIGHTNESS = 100;      // steady playback brightness (lowered to free supply headroom)
const uint8_t LED_BRIGHTNESS_MIN = 20;   // dim level held while the motor draws its inrush current
const uint16_t LED_UPDATE_INTERVAL = 20;

CRGB leds[NUM_LEDS];

// ============================================================
// LED PATTERNS
// ============================================================

enum LEDPattern {
  PATTERN_VINYL_RUNNER,
  PATTERN_COMET,
  PATTERN_DUAL_ORBIT,
  PATTERN_ROTATING_PULSE,
  PATTERN_COMET_SPARKLES,
  PATTERN_BREATHING_ORBIT,
  PATTERN_EXPANDING_RING,
  PATTERN_TWO_COLOR_CHASE,
  PATTERN_CENTERED_COMET,
  PATTERN_RANDOM_FIREFLIES,
  PATTERN_WAVE_AROUND_VINYL,
  PATTERN_HEARTBEAT_ORBIT,
  PATTERN_COUNT
};

// ============================================================
// PATTERN NAMES
// ============================================================

const char* patternNames[PATTERN_COUNT] = {
  "Vinyl Runner",
  "Comet",
  "Dual Orbit",
  "Rotating Pulse",
  "Comet + Sparkles",
  "Breathing Orbit",
  "Expanding Ring",
  "Two Color Chase",
  "Centered Comet",
  "Random Fireflies",
  "Wave Around Vinyl",
  "Heartbeat Orbit"
};

// ============================================================
// ACTIVE RANDOM PATTERNS
// ============================================================

const LEDPattern activePatterns[] = {
  PATTERN_VINYL_RUNNER,
  PATTERN_COMET,
  PATTERN_DUAL_ORBIT,
  PATTERN_ROTATING_PULSE,
  PATTERN_COMET_SPARKLES,
  PATTERN_BREATHING_ORBIT,
  PATTERN_EXPANDING_RING,
  PATTERN_TWO_COLOR_CHASE,
  PATTERN_CENTERED_COMET,
  PATTERN_RANDOM_FIREFLIES,
  PATTERN_WAVE_AROUND_VINYL,
  PATTERN_HEARTBEAT_ORBIT
};

const uint8_t NUM_ACTIVE_PATTERNS = sizeof(activePatterns) / sizeof(activePatterns[0]);

// ============================================================
// LED STATE
// ============================================================

enum LEDMode {
  LED_STANDBY,
  LED_STARTING,
  LED_PLAYING,
  LED_STOPPING,
  LED_LOOP
};

LEDMode ledMode = LED_STANDBY;
LEDPattern currentPattern = PATTERN_VINYL_RUNNER;
LEDPattern lastPattern = PATTERN_COUNT;

uint32_t ledModeStart = 0;
uint32_t lastLEDUpdate = 0;

// ============================================================
// HARDWARE OBJECTS
// ============================================================

SPIClass sdSPI(HSPI);
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioFileSourceBuffer *audioBuffer = nullptr;
AudioOutputI2S *out = nullptr;

// ============================================================
// GLOBALS
// ============================================================

char currentUID[15] = "";
char playingFile[40] = "";
bool isPlaying = false;
bool playbackPending = false;
bool systemReady = false;
bool shutdownPending = false;
uint32_t motorRampStart = 0;
uint32_t lastMotorUpdate = 0;
uint8_t loopCount = 0;
uint8_t volume = INITIAL_VOLUME;
uint32_t audioFadeStart = 0;
bool audioFadingIn = false;
uint32_t lastKeepAlive = 0;
esp_timer_handle_t keyReleaseTimer = nullptr;
uint32_t buttonPressStart = 0;
uint32_t buttonChangedAt = 0;
bool buttonRawPressed = false;
bool buttonStablePressed = false;
int lastClkState = HIGH;
uint32_t lastEncoderTurn = 0;
uint32_t lastActivity = 0;

// ============================================================
// NFC TASK GLOBALS
// ============================================================

struct NFCPresence {
  char uid[15];
};

QueueHandle_t nfcQueue = nullptr;

// ============================================================
// LED HELPERS
// ============================================================

void clearLEDs() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void setLEDMode(LEDMode mode) {
  ledMode = mode;
  ledModeStart = millis();
}

void selectRandomPattern() {
  if (NUM_ACTIVE_PATTERNS == 0) {
    currentPattern = PATTERN_VINYL_RUNNER;
    return;
  }

  if (NUM_ACTIVE_PATTERNS == 1) {
    currentPattern = activePatterns[0];
    return;
  }

  LEDPattern newPattern;
  do {
    uint32_t r = esp_random();
    newPattern = activePatterns[r % NUM_ACTIVE_PATTERNS];
  } while (newPattern == lastPattern);

  lastPattern = newPattern;
  currentPattern = newPattern;

  Serial.print("LED pattern: ");
  Serial.println(patternNames[currentPattern]);
}

// ============================================================
// LED PATTERNS IMPLEMENTATIONS
// ============================================================

void updateVinylRunner() {
  fadeToBlackBy(leds, NUM_LEDS, 35);
  uint16_t pos = (millis() / 70) % NUM_LEDS;

  leds[pos] += CRGB(120, 180, 255);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(50, 80, 120);
  leds[(pos + NUM_LEDS - 2) % NUM_LEDS] += CRGB(20, 35, 60);
}

void updateComet() {
  fadeToBlackBy(leds, NUM_LEDS, 50);
  uint16_t pos = (millis() / 55) % NUM_LEDS;

  leds[pos] = CRGB(180, 220, 255);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(100, 130, 180);
  leds[(pos + NUM_LEDS - 2) % NUM_LEDS] += CRGB(45, 60, 100);
  leds[(pos + NUM_LEDS - 3) % NUM_LEDS] += CRGB(15, 20, 40);
}

void updateDualOrbit() {
  fadeToBlackBy(leds, NUM_LEDS, 42);
  uint16_t pos = (millis() / 70) % NUM_LEDS;
  uint16_t opposite = (pos + NUM_LEDS / 2) % NUM_LEDS;

  leds[pos] = CRGB(120, 200, 255);
  leds[opposite] = CRGB(255, 100, 180);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(50, 80, 120);
  leds[(opposite + 1) % NUM_LEDS] += CRGB(100, 40, 80);
}

void updateRotatingPulse() {
  uint8_t wave = beatsin8(18, 35, 150);
  fill_solid(leds, NUM_LEDS, CRGB(wave / 2, wave, 255));

  uint16_t pos = (millis() / 90) % NUM_LEDS;
  leds[pos] = CRGB::White;
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] = CRGB(120, 180, 220);
}

void updateCometSparkles() {
  fadeToBlackBy(leds, NUM_LEDS, 45);
  uint16_t pos = (millis() / 60) % NUM_LEDS;

  leds[pos] = CRGB(180, 230, 255);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(90, 130, 180);
  leds[(pos + NUM_LEDS - 2) % NUM_LEDS] += CRGB(40, 60, 90);

  if (random8() < 35) {
    uint8_t spark = random8(NUM_LEDS);
    leds[spark] += CRGB(180, 180, 255);
  }
}

void updateBreathingOrbit() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  uint8_t brightness = beatsin8(12, 35, 180);
  uint16_t pos = (millis() / 95) % NUM_LEDS;

  leds[pos] = CRGB(brightness / 2, brightness, brightness);
  leds[(pos + 1) % NUM_LEDS] += CRGB(20, 40, 50);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(20, 40, 50);
}

void updateExpandingRing() {
  fadeToBlackBy(leds, NUM_LEDS, 55);
  uint16_t phase = (millis() / 120) % NUM_LEDS;
  uint16_t center = 0;

  uint16_t a = (center + phase) % NUM_LEDS;
  uint16_t b = (center + NUM_LEDS - phase) % NUM_LEDS;

  leds[a] += CRGB(120, 180, 255);
  leds[b] += CRGB(120, 180, 255);
}

void updateTwoColorChase() {
  fadeToBlackBy(leds, NUM_LEDS, 38);
  uint16_t pos = (millis() / 80) % NUM_LEDS;
  uint16_t pos2 = (pos + NUM_LEDS / 2) % NUM_LEDS;

  leds[pos] = CRGB(255, 80, 120);
  leds[pos2] = CRGB(80, 160, 255);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(100, 30, 50);
  leds[(pos2 + NUM_LEDS - 1) % NUM_LEDS] += CRGB(30, 60, 100);
}

void updateCenteredComet() {
  fadeToBlackBy(leds, NUM_LEDS, 42);
  uint16_t center = (millis() / 70) % NUM_LEDS;

  leds[center] = CRGB(220, 240, 255);
  leds[(center + 1) % NUM_LEDS] += CRGB(100, 150, 200);
  leds[(center + NUM_LEDS - 1) % NUM_LEDS] += CRGB(100, 150, 200);
  leds[(center + 2) % NUM_LEDS] += CRGB(40, 70, 100);
  leds[(center + NUM_LEDS - 2) % NUM_LEDS] += CRGB(40, 70, 100);
}

void updateRandomFireflies() {
  fadeToBlackBy(leds, NUM_LEDS, 25);

  if (random8() < 70) {
    uint8_t pos = random8(NUM_LEDS);
    leds[pos] += CRGB(random8(60, 150), random8(80, 180), 255);
  }
}

void updateWaveAroundVinyl() {
  uint16_t t = millis() / 18;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t wave = sin8((i * 255 / NUM_LEDS) + t);
    uint8_t brightness = scale8(wave, 150);
    leds[i] = CRGB(brightness / 3, brightness, brightness);
  }
}

void updateHeartbeatOrbit() {
  fadeToBlackBy(leds, NUM_LEDS, 35);
  uint16_t pos = (millis() / 90) % NUM_LEDS;
  uint8_t pulse = beatsin8(28, 50, 220);

  leds[pos] = CRGB(pulse, pulse / 3, pulse / 2);
  leds[(pos + NUM_LEDS - 1) % NUM_LEDS] += CRGB(pulse / 3, pulse / 8, pulse / 6);
}

// ============================================================
// UPDATE ACTIVE PLAYING PATTERN
// ============================================================

void updatePlayingPattern() {
  switch (currentPattern) {
    case PATTERN_VINYL_RUNNER:     updateVinylRunner(); break;
    case PATTERN_COMET:            updateComet(); break;
    case PATTERN_DUAL_ORBIT:        updateDualOrbit(); break;
    case PATTERN_ROTATING_PULSE:   updateRotatingPulse(); break;
    case PATTERN_COMET_SPARKLES:   updateCometSparkles(); break;
    case PATTERN_BREATHING_ORBIT:  updateBreathingOrbit(); break;
    case PATTERN_EXPANDING_RING:   updateExpandingRing(); break;
    case PATTERN_TWO_COLOR_CHASE:  updateTwoColorChase(); break;
    case PATTERN_CENTERED_COMET:   updateCenteredComet(); break;
    case PATTERN_RANDOM_FIREFLIES: updateRandomFireflies(); break;
    case PATTERN_WAVE_AROUND_VINYL:updateWaveAroundVinyl(); break;
    case PATTERN_HEARTBEAT_ORBIT:  updateHeartbeatOrbit(); break;
    default:                       updateVinylRunner(); break;
  }
}

// ============================================================
// LED MODES
// ============================================================

void updateLEDStandby() {
  // Slow, smooth breathing fade so the ring is clearly "alive" while idle.
  uint8_t breath = beatsin8(6, 3, 40);  // ~6 bpm: gentle rise and fall
  fill_solid(leds, NUM_LEDS, CRGB(breath / 4, breath / 2, breath));
}

void updateLEDStarting() {
  uint32_t elapsed = millis() - ledModeStart;
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Ramp master brightness up alongside the ring fill so LED current rises gradually, not all at once.
  FastLED.setBrightness(map(min(elapsed, (uint32_t)1000), 0, 1000, LED_BRIGHTNESS_MIN, LED_BRIGHTNESS));

  uint8_t count = map(min(elapsed, (uint32_t)1000), 0, 1000, 0, NUM_LEDS);

  for (uint8_t i = 0; i < count; i++) {
    uint8_t brightness = map(i, 0, max((uint8_t)1, count), 60, 255);
    leds[i] = CRGB(brightness / 2, brightness, 255);
  }

  if (elapsed >= 1000) {
    FastLED.setBrightness(LED_BRIGHTNESS);
    setLEDMode(LED_PLAYING);
  }
}

void updateLEDLoop() {
  uint32_t elapsed = millis() - ledModeStart;
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  uint8_t brightness = 0;
  if (elapsed < 150) {
    brightness = 255;
  } else if (elapsed < 400) {
    brightness = map(elapsed, 150, 400, 255, 0);
  }

  if (brightness > 0) {
    fill_solid(leds, NUM_LEDS, CRGB(brightness, brightness / 2, 20));
  }

  if (elapsed >= 400) {
    setLEDMode(LED_PLAYING);
  }
}

void updateLEDStopping() {
  uint32_t elapsed = millis() - ledModeStart;

  if (elapsed < 500) {
    uint8_t brightness = map(elapsed, 0, 500, 255, 0);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds[i].nscale8(brightness);
    }
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    setLEDMode(LED_STANDBY);  // resume idle breathing after the fade-out completes
  }
}

void updateLEDs() {
  if (playbackPending || shutdownPending) return;
  uint32_t now = millis();
  if (now - lastLEDUpdate < LED_UPDATE_INTERVAL) {
    return;
  }
  lastLEDUpdate = now;

  // Keep full brightness in every mode except the startup ramp, which manages it itself.
  if (ledMode != LED_STARTING) {
    FastLED.setBrightness(LED_BRIGHTNESS);
  }

  switch (ledMode) {
    case LED_STANDBY:  updateLEDStandby(); break;
    case LED_STARTING: updateLEDStarting(); break;
    case LED_PLAYING:  updatePlayingPattern(); break;
    case LED_LOOP:     updateLEDLoop(); break;
    case LED_STOPPING: updateLEDStopping(); break;
  }

  FastLED.show();
}

// ============================================================
// SYSTEM CONTROLS
// ============================================================

void writeMotorDuty(uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(MOTOR_PIN, duty);
#else
  ledcWrite(MOTOR_PWM_CHANNEL, duty);
#endif
}

bool setupMotorPWM() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(MOTOR_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES)) return false;
#else
  if (ledcSetup(MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQ, MOTOR_PWM_RES) == 0) return false;
  ledcAttachPin(MOTOR_PIN, MOTOR_PWM_CHANNEL);
#endif
  writeMotorDuty(0);
  return true;
}

void motorOff() {
  writeMotorDuty(0);
}

void softStartMotor() {
  Serial.println("Soft-starting motor...");
  motorRampStart = millis();
  lastMotorUpdate = motorRampStart;
  playbackPending = true;
  writeMotorDuty(MOTOR_DUTY_MIN);
}

void stopPlayback() {
  Serial.println("Stopping playback...");

  digitalWrite(AMP_SD, LOW);
  motorOff();
  playbackPending = false;
  isPlaying = false;
  audioFadingIn = false;
  if (out != nullptr) out->SetGain(0.0f);

  if (mp3 != nullptr) {
    if (mp3->isRunning()) {
      mp3->stop();
    }
    delete mp3;
    mp3 = nullptr;
  }

  if (audioBuffer != nullptr) {
    delete audioBuffer;
    audioBuffer = nullptr;
  }

  if (file != nullptr) {
    file->close();
    delete file;
    file = nullptr;
  }

  Serial.println("Playback stopped.");
}

void releaseKey(void *parameter) {
  digitalWrite(IP5310_KEY, LOW);
}

void stopKeepAlive() {
  if (keyReleaseTimer != nullptr) esp_timer_stop(keyReleaseTimer);
  digitalWrite(IP5310_KEY, LOW);
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  stopKeepAlive();
  stopPlayback();
  clearLEDs();
  delay(200);

  // Hold the encoder switch HIGH during sleep so a press pulls it LOW and wakes the ESP32.
  rtc_gpio_pullup_en((gpio_num_t)ENCODER_SW);
  rtc_gpio_pulldown_dis((gpio_num_t)ENCODER_SW);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_SW, 0);  // wake on LOW = button pressed

  Serial.flush();
  esp_deep_sleep_start();
}

bool readNFCUID(char *uid, size_t uidSize) {
  uint8_t uidBuffer[10];
  uint8_t uidLength = 0;

  bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBuffer, &uidLength, 50);

  if (!success || uidLength == 0 || uidLength > 7 || uidSize < uidLength * 2U + 1U) {
    return false;
  }

  size_t pos = 0;
  for (uint8_t i = 0; i < uidLength && pos + 2 < uidSize; i++) {
    pos += snprintf(uid + pos, uidSize - pos, "%02X", uidBuffer[i]);
  }
  uid[pos] = '\0';

  return true;
}

bool playTrack(const char *filename) {
  Serial.print("Starting playback: ");
  Serial.println(filename);

  stopPlayback();

  strncpy(playingFile, filename, sizeof(playingFile) - 1);
  playingFile[sizeof(playingFile) - 1] = '\0';

  if (!SD.exists(playingFile)) {
    Serial.print("File not found: ");
    Serial.println(playingFile);
    return false;
  }

  clearLEDs();
  softStartMotor();
  return true;
}

void updatePlaybackStartup() {
  if (!playbackPending) return;
  uint32_t now = millis();
  uint32_t elapsed = now - motorRampStart;
  if (elapsed < MOTOR_SOFT_START_MS) {
    if (now - lastMotorUpdate >= MOTOR_RAMP_STEP_MS) {
      lastMotorUpdate = now;
      writeMotorDuty(static_cast<uint8_t>(map(elapsed, 0, MOTOR_SOFT_START_MS, MOTOR_DUTY_MIN, MOTOR_RUN_DUTY)));
    }
    return;
  }

  playbackPending = false;
  writeMotorDuty(MOTOR_RUN_DUTY);
  out->SetGain(0.0f);
  file = new (std::nothrow) AudioFileSourceSD(playingFile);
  if (file != nullptr) audioBuffer = new (std::nothrow) AudioFileSourceBuffer(file, AUDIO_BUFFER_SIZE);
  if (audioBuffer != nullptr) mp3 = new (std::nothrow) AudioGeneratorMP3();

  if (mp3 == nullptr || !mp3->begin(audioBuffer, out)) {
    Serial.println("MP3 begin failed.");
    stopPlayback();
    setLEDMode(LED_STANDBY);
    return;
  }

  setLEDMode(LED_STARTING);
  digitalWrite(AMP_SD, HIGH);
  isPlaying = true;
  audioFadeStart = millis();
  audioFadingIn = true;
  Serial.println("MP3 playback started.");
}

// ============================================================
// NFC TASK
// ============================================================

void nfcTask(void *parameter) {
  Serial.println("NFC task started on Core 0.");
  NFCPresence presence = {};
  uint8_t missingReads = 0;

  for (;;) {
    char detectedUID[15] = "";
    if (readNFCUID(detectedUID, sizeof(detectedUID))) {
      missingReads = 0;
      if (strcmp(detectedUID, presence.uid) != 0) {
        strncpy(presence.uid, detectedUID, sizeof(presence.uid) - 1);
        presence.uid[sizeof(presence.uid) - 1] = '\0';
        xQueueOverwrite(nfcQueue, &presence);
      }
    } else if (presence.uid[0] != '\0') {
      if (++missingReads >= NFC_MISSING_LIMIT) {
        presence.uid[0] = '\0';
        missingReads = 0;
        xQueueOverwrite(nfcQueue, &presence);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(NFC_SCAN_INTERVAL));
  }
}

// ============================================================
// ENCODER
// ============================================================

// Map the 0..MAX_VOLUME detent to a perceptual (square-law) gain curve.
float volumeToGain(uint8_t vol) {
  if (vol == 0) return 0.0f;
  float t = (float)vol / (float)MAX_VOLUME;  // 0..1 linear step
  return t * t;                              // approximates loudness perception
}

// 0..1 fade multiplier applied on top of the volume gain; reaches 1.0 after AUDIO_FADE_IN_MS.
float audioFadeScale() {
  if (!audioFadingIn) return 1.0f;
  uint32_t elapsed = millis() - audioFadeStart;
  if (elapsed >= AUDIO_FADE_IN_MS) {
    audioFadingIn = false;
    return 1.0f;
  }
  return (float)elapsed / (float)AUDIO_FADE_IN_MS;
}

void applyGain() {
  if (out != nullptr) {
    out->SetGain(volumeToGain(volume) * audioFadeScale());
  }
}

void updateKeepAlive() {
  if (!IP5310_KEEPALIVE_ENABLED || shutdownPending || keyReleaseTimer == nullptr) return;

  uint32_t now = millis();
  if (esp_timer_is_active(keyReleaseTimer)) return;

  if (now - lastKeepAlive >= IP5310_KEEPALIVE_INTERVAL) {
    lastKeepAlive = now;
    digitalWrite(IP5310_KEY, HIGH);
    if (esp_timer_start_once(keyReleaseTimer, (uint64_t)IP5310_KEEPALIVE_PULSE_MS * 1000ULL) != ESP_OK) {
      digitalWrite(IP5310_KEY, LOW);
      Serial.println("KEY release timer failed; pulse cancelled.");
    }
  }
}

void handlePowerButton() {
  bool pressed = (digitalRead(ENCODER_SW) == LOW);
  uint32_t now = millis();
  if (pressed != buttonRawPressed) {
    buttonRawPressed = pressed;
    buttonChangedAt = now;
  }
  if (now - buttonChangedAt < BUTTON_DEBOUNCE_MS) return;
  if (buttonStablePressed != buttonRawPressed) {
    buttonStablePressed = buttonRawPressed;
    if (buttonStablePressed) {
      buttonPressStart = now;
    } else if (shutdownPending) {
      enterDeepSleep();
    }
  }
  if (buttonStablePressed) {
    lastActivity = now;
    if (!shutdownPending && now - buttonPressStart >= LONG_PRESS_MS) {
      shutdownPending = true;
      stopKeepAlive();
      stopPlayback();
      clearLEDs();
      Serial.println("Sleep requested; release encoder button.");
    }
  }
}

void handleEncoder() {
  int clkState = digitalRead(ENCODER_CLK);

  if (clkState == LOW && lastClkState == HIGH) {
    uint32_t now = millis();
    if (now - lastEncoderTurn >= 2) {  // debounce spurious edges
      lastEncoderTurn = now;
      lastActivity = now;
      int dtState = digitalRead(ENCODER_DT);

      if (dtState != clkState) {
        if (volume < MAX_VOLUME) volume++;
      } else {
        if (volume > MIN_VOLUME) volume--;
      }

      if (out != nullptr) {
        applyGain();
      }

      Serial.print("Volume: ");
      Serial.println(volume);
    }
  }

  lastClkState = clkState;
}

// ============================================================
// SETUP
// ============================================================

// Drive every actuator to its safe/off state; safe to call on any (re)boot.
void forceOutputsSafe() {
  digitalWrite(MOTOR_PIN, LOW);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(AMP_SD, LOW);
  pinMode(AMP_SD, OUTPUT);
  digitalWrite(IP5310_KEY, LOW);
  pinMode(IP5310_KEY, OUTPUT);
}

void setup() {
  // Put actuators in their safe state first, so a brown-out/charger-glitch reset can't leave
  // the amp, motor, or KEY tap spuriously driven during the transient.
  forceOutputsSafe();

  Serial.begin(115200);
  delay(500);  // let the rail settle after a power-transient reset
  Serial.printf("Reset reason: %d; wake cause: %d\n",
                (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());

  // Motor (PWM for soft-start)
  if (!setupMotorPWM()) {
    Serial.println("Motor PWM initialization failed; restart required.");
    return;
  }

  // Encoder
  rtc_gpio_deinit((gpio_num_t)ENCODER_SW);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // LEDs
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Random seed
  randomSeed(esp_random());
  random16_set_seed(esp_random());  // seed FastLED RNG so sparkle patterns differ each boot

  // SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI, SD_SPEED)) {
    Serial.println("SD card initialization failed!");
    return;
  } else {
    Serial.println("SD card initialized.");
    if (!SD.exists("/player")) {
      SD.mkdir("/player");
    }
  }

  // PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();

  if (!versiondata) {
    Serial.println("Didn't find PN532.");
    return;
  } else {
    Serial.println("PN532 found.");
    nfc.setPassiveActivationRetries(0x01);
    if (!nfc.SAMConfig()) {
      Serial.println("PN532 configuration failed.");
      return;
    }
  }

  // I2S
  out = new (std::nothrow) AudioOutputI2S();
  if (out == nullptr) {
    Serial.println("Audio allocation failed.");
    return;
  }
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(0.0f);

  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = releaseKey;
  timerArgs.name = "key_release";
  if (esp_timer_create(&timerArgs, &keyReleaseTimer) != ESP_OK) {
    Serial.println("KEY timer initialization failed.");
    return;
  }

  nfcQueue = xQueueCreate(1, sizeof(NFCPresence));
  if (nfcQueue == nullptr ||
      xTaskCreatePinnedToCore(nfcTask, "NFC_Task", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("NFC task initialization failed.");
    return;
  }

  // Initial LED state
  setLEDMode(LED_STANDBY);
  lastActivity = millis();
  lastKeepAlive = millis();
  systemReady = true;

  Serial.println("System ready.");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  if (!systemReady) {
    delay(10);
    return;
  }
  handlePowerButton();
  if (shutdownPending) {
    delay(1);
    return;
  }
  updateKeepAlive();

  NFCPresence presence = {};
  if (xQueueReceive(nfcQueue, &presence, 0) == pdTRUE) {
    lastActivity = millis();
    strncpy(currentUID, presence.uid, sizeof(currentUID) - 1);
    currentUID[sizeof(currentUID) - 1] = '\0';
    if (currentUID[0] == '\0') {
      stopPlayback();
      setLEDMode(LED_STOPPING);
    } else {
      char filename[40];
      snprintf(filename, sizeof(filename), "/player/%s.mp3", currentUID);
      loopCount = 0;
      selectRandomPattern();
      if (!playTrack(filename)) setLEDMode(LED_STANDBY);
    }
  }

  updatePlaybackStartup();
  if (playbackPending) lastActivity = millis();

  // MP3 playback
  if (isPlaying && mp3 != nullptr) {
    lastActivity = millis();
    if (!mp3->isRunning() || !mp3->loop()) {
      Serial.println("Track finished.");

      mp3->stop();
      isPlaying = false;
      loopCount++;

      if (loopCount < MAX_LOOPS) {
        Serial.print("Loop ");
        Serial.print(loopCount + 1);
        Serial.print(" / ");
        Serial.println(MAX_LOOPS);

        char replayFile[40];
        strncpy(replayFile, playingFile, sizeof(replayFile) - 1);
        replayFile[sizeof(replayFile) - 1] = '\0';

        setLEDMode(LED_LOOP);

        if (!playTrack(replayFile)) {
          Serial.println("Replay failed.");
          stopPlayback();
          setLEDMode(LED_STANDBY);
        }
      } else {
        stopPlayback();
        setLEDMode(LED_STANDBY);
        lastActivity = millis();  // begin idle countdown instead of sleeping immediately
      }
    }
  }

  // Controls & LED Update
  if (audioFadingIn) {
    applyGain();  // advance the startup volume ramp
  }
  handleEncoder();
  updateLEDs();

  // Idle auto-sleep: fires in both cases (tag removed or 3 loops elapsed)
  if (!isPlaying && !playbackPending && !buttonRawPressed &&
      !buttonStablePressed && (millis() - lastActivity >= IDLE_SLEEP_TIMEOUT)) {
    enterDeepSleep();
  }

  delay(1);
}
