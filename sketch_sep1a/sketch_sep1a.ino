#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_PN532.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// =============================
// ПИНОВЕ
// =============================

// SD (HSPI)
#define SD_CS   13
#define SD_SCK  14
#define SD_MISO 16
#define SD_MOSI 15

// PN532 (VSPI)
#define PN532_SS   5
#define PN532_SCK  18
#define PN532_MISO 19
#define PN532_MOSI 23

// MAX98357A
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// Мотор
#define MOTOR_PIN 27

// Енкодер
#define ENCODER_CLK 32
#define ENCODER_DT  33
#define ENCODER_SW   4

const uint8_t MAX_LOOPS = 3;

// =============================
// ОБЕКТИ
// =============================

SPIClass sdSPI(HSPI);
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputI2S *out = nullptr;

// =============================
// ПРОМЕНЛИВИ
// =============================

char currentUID[15] = "";
char playingFile[40] = "";

bool isPlaying = false;
uint8_t loopCount = 0;

uint8_t volume = 12;
int lastClkState;
uint8_t missingReads = 0;

// =============================
// ФУНКЦИИ
// =============================

void startMotor() {
  digitalWrite(MOTOR_PIN, HIGH);
}

void stopMotor() {
  digitalWrite(MOTOR_PIN, LOW);
}

void stopPlayback() {

  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }

  if (file) {
    delete file;
    file = nullptr;
  }

  stopMotor();

  isPlaying = false;
}

void enterDeepSleep() {

  Serial.println("[SLEEP] Deep Sleep");

  stopPlayback();

  delay(200);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_SW, 0);
  esp_deep_sleep_start();
}

bool readNFCUID(char *uidString) {

  uint8_t uid[7];
  uint8_t uidLength;

  if (!nfc.readPassiveTargetID(
        PN532_MIFARE_ISO14443A,
        uid,
        &uidLength,
        50))
    return false;

  char *p = uidString;

  for (uint8_t i = 0; i < uidLength; i++) {
    sprintf(p, "%02X", uid[i]);
    p += 2;
  }

  *p = '\0';

  return true;
}

void playTrack(const char *path) {

  stopPlayback();

  strcpy(playingFile, path);

  startMotor();
  delay(500);                 // реалистично развъртане

  file = new AudioFileSourceSD(path);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(volume / 21.0f);

  mp3 = new AudioGeneratorMP3();

  if (mp3->begin(file, out)) {

    isPlaying = true;
    loopCount = 0;

    Serial.print("[PLAY] ");
    Serial.println(path);

  } else {

    Serial.println("[ERROR] MP3 start failed");
    stopPlayback();
  }
}

// =============================
// SETUP
// =============================

void setup() {

  Serial.begin(115200);
  delay(500);

  Serial.println("=== NFC Vinyl Player ===");

  pinMode(MOTOR_PIN, OUTPUT);
  stopMotor();

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  lastClkState = digitalRead(ENCODER_CLK);

  // SD

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI))
    Serial.println("[ERROR] SD");
  else
    Serial.println("[OK] SD");

  // NFC

  nfc.begin();

  if (!nfc.getFirmwareVersion()) {

    Serial.println("[ERROR] PN532");

  } else {

    nfc.SAMConfig();
    nfc.setPassiveActivationRetries(0x11);

    Serial.println("[OK] PN532");
  }
}

// =============================
// LOOP
// =============================

void loop() {

  // MP3

  if (isPlaying && mp3) {

    if (mp3->isRunning()) {

      mp3->loop();

    } else {

      loopCount++;

      Serial.printf("[LOOP] %d/%d\n", loopCount, MAX_LOOPS);

      if (loopCount < MAX_LOOPS) {

        playTrack(playingFile);

      } else {

        enterDeepSleep();
      }
    }
  }

  // Енкодер

  int clk = digitalRead(ENCODER_CLK);

  if (clk != lastClkState && clk == LOW) {

    if (digitalRead(ENCODER_DT) != clk)
      volume = min((uint8_t)21, (uint8_t)(volume + 1));
    else
      volume = max((uint8_t)0, (uint8_t)(volume - 1));

    if (out)
      out->SetGain(volume / 21.0f);

    Serial.printf("[VOL] %d\n", volume);
  }

  lastClkState = clk;

  // NFC

  static uint32_t lastScan = 0;

  if (millis() - lastScan < 250)
    return;

  lastScan = millis();

  char detectedUID[15];

  if (readNFCUID(detectedUID)) {

    missingReads = 0;

    if (strcmp(detectedUID, currentUID) != 0) {

      strcpy(currentUID, detectedUID);

      char filename[40];

      snprintf(
        filename,
        sizeof(filename),
        "/player/%s.mp3",
        currentUID);

      if (SD.exists(filename)) {

        playTrack(filename);

      } else {

        Serial.print("[NOT FOUND] ");
        Serial.println(filename);
      }
    }

  } else {

    if (++missingReads >= 3 && currentUID[0]) {

      Serial.println("[STOP] Record removed");

      stopPlayback();

      currentUID[0] = '\0';
      playingFile[0] = '\0';

      loopCount = 0;
      missingReads = 0;
    }
  }
}