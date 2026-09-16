#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_PN532.h>
#include <FastLED.h>
#include "driver/rtc_io.h"

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

// ============================================================
// SETTINGS
// ============================================================

const uint8_t MAX_LOOPS = 3;
const uint32_t SD_SPEED = 10000000;
const size_t AUDIO_BUFFER_SIZE = 8192;

const uint32_t NFC_SCAN_IDLE = 150;
const uint32_t NFC_SCAN_PLAYING = 200;
const uint8_t NFC_MISSING_LIMIT = 2;

const uint32_t IDLE_SLEEP_TIMEOUT = 5UL * 60UL * 1000UL;  // deep sleep after 5 min of inactivity

const uint8_t INITIAL_VOLUME = 12;
const uint8_t MIN_VOLUME = 0;
const uint8_t MAX_VOLUME = 21;

// ============================================================
// LED SETTINGS
// ============================================================

const uint8_t LED_BRIGHTNESS = 120;
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
  PATTERN_BREATHING_ORBIT
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
volatile bool isPlaying = false;
uint8_t loopCount = 0;
uint8_t volume = INITIAL_VOLUME;
int lastClkState = HIGH;
uint32_t lastEncoderTurn = 0;
uint32_t lastActivity = 0;

// ============================================================
// NFC TASK GLOBALS
// ============================================================

volatile bool nfcTaskRunning = true;
volatile bool nfcTagDetected = false;
volatile bool nfcTagRemoved = false;
char nfcUID[15] = "";
uint8_t missingReads = 0;

TaskHandle_t nfcTaskHandle = nullptr;
SemaphoreHandle_t nfcMutex = nullptr;

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
  uint8_t brightness = beatsin8(8, 5, 28);
  fill_solid(leds, NUM_LEDS, CRGB(brightness / 3, brightness / 2, brightness));
}

void updateLEDStarting() {
  uint32_t elapsed = millis() - ledModeStart;
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  uint8_t count = map(min(elapsed, (uint32_t)1000), 0, 1000, 0, NUM_LEDS);

  for (uint8_t i = 0; i < count; i++) {
    uint8_t brightness = map(i, 0, max((uint8_t)1, count), 60, 255);
    leds[i] = CRGB(brightness / 2, brightness, 255);
  }

  if (elapsed >= 1000) {
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
  }
}

void updateLEDs() {
  uint32_t now = millis();
  if (now - lastLEDUpdate < LED_UPDATE_INTERVAL) {
    return;
  }
  lastLEDUpdate = now;

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

void stopPlayback() {
  Serial.println("Stopping playback...");

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

  digitalWrite(MOTOR_PIN, LOW);
  isPlaying = false;
  Serial.println("Playback stopped.");
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  digitalWrite(MOTOR_PIN, LOW);
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

  if (!success || uidLength == 0 || uidLength > 7) {
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

  digitalWrite(MOTOR_PIN, HIGH);
  delay(300);

  file = new AudioFileSourceSD(playingFile);
  audioBuffer = new AudioFileSourceBuffer(file, AUDIO_BUFFER_SIZE);
  mp3 = new AudioGeneratorMP3();

  if (!mp3->begin(audioBuffer, out)) {
    Serial.println("MP3 begin failed.");
    stopPlayback();
    return false;
  }

  isPlaying = true;
  Serial.println("MP3 playback started.");
  return true;
}

// ============================================================
// NFC TASK
// ============================================================

// Update the active UID under the mutex (owned by loop(), read by nfcTask on core 0).
void setCurrentUID(const char *uid) {
  if (xSemaphoreTake(nfcMutex, portMAX_DELAY) == pdTRUE) {
    strncpy(currentUID, uid, sizeof(currentUID) - 1);
    currentUID[sizeof(currentUID) - 1] = '\0';
    xSemaphoreGive(nfcMutex);
  }
}

void nfcTask(void *parameter) {
  Serial.println("NFC task started on Core 0.");

  while (nfcTaskRunning) {
    char detectedUID[15] = "";

    // Snapshot the active UID under the mutex to avoid a cross-core data race.
    char activeUID[15] = "";
    if (xSemaphoreTake(nfcMutex, portMAX_DELAY) == pdTRUE) {
      strncpy(activeUID, currentUID, sizeof(activeUID) - 1);
      activeUID[sizeof(activeUID) - 1] = '\0';
      xSemaphoreGive(nfcMutex);
    }

    if (readNFCUID(detectedUID, sizeof(detectedUID))) {
      missingReads = 0;

      if (activeUID[0] == '\0' || strcmp(detectedUID, activeUID) != 0) {
        Serial.print("NFC detected: ");
        Serial.println(detectedUID);

        if (xSemaphoreTake(nfcMutex, portMAX_DELAY) == pdTRUE) {
          strncpy(nfcUID, detectedUID, sizeof(nfcUID) - 1);
          nfcUID[sizeof(nfcUID) - 1] = '\0';
          nfcTagDetected = true;
          xSemaphoreGive(nfcMutex);
        }
      }
    } else {
      if (activeUID[0] != '\0') {
        missingReads++;

        if (missingReads >= NFC_MISSING_LIMIT) {
          Serial.println("NFC tag removed.");

          if (xSemaphoreTake(nfcMutex, portMAX_DELAY) == pdTRUE) {
            nfcTagRemoved = true;
            xSemaphoreGive(nfcMutex);
          }
          missingReads = 0;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(isPlaying ? NFC_SCAN_PLAYING : NFC_SCAN_IDLE));
  }

  vTaskDelete(nullptr);
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
        out->SetGain(volumeToGain(volume));
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Motor
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  // Encoder
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

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
  } else {
    Serial.println("PN532 found.");
    nfc.SAMConfig();
  }

  // I2S
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(volumeToGain(volume));

  // FreeRTOS Synchronization
  nfcMutex = xSemaphoreCreateMutex();

  // NFC task on Core 0
  xTaskCreatePinnedToCore(nfcTask, "NFC_Task", 4096, nullptr, 1, &nfcTaskHandle, 0);

  // Initial LED state
  setLEDMode(LED_STANDBY);
  lastActivity = millis();

  Serial.println("System ready.");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  // MP3 playback
  if (isPlaying && mp3 != nullptr && mp3->isRunning()) {
    lastActivity = millis();
    if (!mp3->loop()) {
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

  // NFC new tag
  bool newTag = false;
  char newUID[15] = "";

  if (xSemaphoreTake(nfcMutex, 0) == pdTRUE) {
    if (nfcTagDetected) {
      strncpy(newUID, nfcUID, sizeof(newUID) - 1);
      newUID[sizeof(newUID) - 1] = '\0';
      nfcTagDetected = false;
      newTag = true;
    }
    xSemaphoreGive(nfcMutex);
  }

  if (newTag) {
    Serial.print("New NFC tag: ");
    Serial.println(newUID);

    lastActivity = millis();
    setCurrentUID(newUID);

    char filename[40];
    snprintf(filename, sizeof(filename), "/player/%s.mp3", currentUID);

    loopCount = 0;

    selectRandomPattern();
    setLEDMode(LED_STARTING);

    if (!playTrack(filename)) {
      Serial.println("Playback failed.");
      setCurrentUID("");
      setLEDMode(LED_STANDBY);
    }
  }

  // NFC tag removed
  bool tagRemoved = false;

  if (xSemaphoreTake(nfcMutex, 0) == pdTRUE) {
    if (nfcTagRemoved) {
      nfcTagRemoved = false;
      tagRemoved = true;
    }
    xSemaphoreGive(nfcMutex);
  }

  if (tagRemoved) {
    Serial.println("Tag removal detected.");
    setCurrentUID("");
    lastActivity = millis();

    setLEDMode(LED_STOPPING);

    if (isPlaying) {
      stopPlayback();
    }
  }

  // Controls & LED Update
  handleEncoder();
  updateLEDs();

  // Idle auto-sleep: fires in both cases (tag removed or 3 loops elapsed)
  if (!isPlaying && (millis() - lastActivity >= IDLE_SLEEP_TIMEOUT)) {
    enterDeepSleep();
  }

  delay(1);
}