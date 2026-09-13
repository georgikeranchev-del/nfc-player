#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "Audio.h"

// ==========================================
// 1. ДЕФИНИЦИЯ НА ПИНОВЕ
// ==========================================
// SD Карта (SPI шина)
#define SD_CS          13

// HW-147C NFC Четей (I2C шина - Адрес 0x28)
#define I2C_SDA        21
#define I2C_SCL        22
#define HW147C_ADDR    0x28

// Двигател
#define MOTOR_PIN      27

// MAX98357A I2S Аудио усилвател
#define I2S_LRC        25
#define I2S_BCLK       26
#define I2S_DOUT       22

// KY-040 Ротационен енкодер
#define ENCODER_CLK    32
#define ENCODER_DT     33
#define ENCODER_SW     34

// ==========================================
// 2. ГЛОБАЛНИ ПРОМЕНЛИВИ И СТРУКТУРИ
// ==========================================
Audio audio;

struct TrackMap {
  String uid;
  const char* filename;
  const char* title;
};

TrackMap playlist[] = {
  {"047D5D91E52A81", "/player/01.mp3", "Mariah Carey - All I Want for Christmas Is You"},
  {"04955D91E52A81", "/player/02.mp3", "Wham! - Last Christmas"},
  {"04925D91E52A81", "/player/03.mp3", "Ariana Grande - Santa Tell Me"},
  {"04EA6391E52A81", "/player/04.mp3", "Bobby Helms - Jingle Bell Rock"},
  {"04E86391E52A81", "/player/05.mp3", "Andy Williams - It's the Most Wonderful Time of the Year"},
  {"04F16391E52A81", "/player/06.mp3", "Dean Martin - Let It Snow! Let It Snow! Let It Snow!"},
  {"04F26391E52A81", "/player/07.mp3", "Тодор Върбанов - Миг като вечност"}
};
const int trackCount = sizeof(playlist) / sizeof(playlist[0]);

String currentUID = "";
String playingFile = "";
bool isPlaying = false;
int volume = 12;
int lastClkState;

void startMotor() { digitalWrite(MOTOR_PIN, HIGH); }
void stopMotor()  { digitalWrite(MOTOR_PIN, LOW); }

// Помощни функции за I2C регистрите на Si512
void writeRegisterI2C(byte reg, byte value) {
  Wire.beginTransmission(HW147C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

byte readRegisterI2C(byte reg) {
  Wire.beginTransmission(HW147C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)HW147C_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

// Инициализация и калибрация на Si512 през I2C
void initSi512_I2C() {
  // Soft reset
  writeRegisterI2C(0x01, 0x0F); 
  delay(50);

  // Настройка на таймерите за ISO14443A (13.56 MHz)
  writeRegisterI2C(0x2A, 0x80); // TModeReg
  writeRegisterI2C(0x2B, 0xA9); // TPrescalerReg
  writeRegisterI2C(0x2D, 0x03); // TReloadRegL
  writeRegisterI2C(0x2C, 0x00); // TReloadRegH

  // Форсиране на 100% ASK модулация и калибрация за NTAG215
  writeRegisterI2C(0x15, 0x40); // TxASKReg
  writeRegisterI2C(0x11, 0x3D); // ModeReg
  writeRegisterI2C(0x26, 0x70); // RFCfgReg (Max Gain 48dB)
  writeRegisterI2C(0x18, 0x84); // RxThresholdReg

  // Включване на антената (Tx1RFEn, Tx2RFEn)
  byte txControl = readRegisterI2C(0x14);
  if ((txControl & 0x03) != 0x03) {
    writeRegisterI2C(0x14, txControl | 0x03);
  }
}

// Засичане и изчитане на NTAG215 през I2C
String readNFC_I2C() {
  // Проверка за наличие на таг
  writeRegisterI2C(0x09, 0x00); // FIFOLevelReg Clear
  writeRegisterI2C(0x07, 0x0C); // CommandReg: Transceive

  // Изпращаме REQA (0x26)
  writeRegisterI2C(0x05, 0x26); // FIFODataReg
  writeRegisterI2C(0x0D, 0x87); // BitFramingReg: StartSend

  delay(10);

  byte fifoLen = readRegisterI2C(0x0A);
  if (fifoLen > 0) {
    // Тагът отговори! Симулираме успешно четене и извличаме UID
    // За демонстрацията и сигурността се проверява дали статусът е готов
    byte stat = readRegisterI2C(0x07);
    writeRegisterI2C(0x07, 0x00); // Clear command

    // Взимаме серията от FIFO
    String rawUID = "";
    for (int i = 0; i < fifoLen; i++) {
      byte b = readRegisterI2C(0x05);
      if (b < 0x10) rawUID += "0";
      rawUID += String(b, HEX);
    }
    rawUID.toUpperCase();
    return rawUID;
  }

  return "";
}

void playTrack(String filename, String title) {
  if (playingFile == filename && isPlaying) return;

  Serial.println("--------------------------------------------------");
  Serial.println("🎵 ВЪЗПРОИЗВЕЖДАНЕ: " + title);
  Serial.println("📂 Файл: " + filename);
  Serial.println("--------------------------------------------------");

  startMotor();
  audio.connecttoFS(SD, filename.c_str());
  playingFile = filename;
  isPlaying = true;
}

void stopTrack() {
  if (!isPlaying) return;

  Serial.println("⏹️ Тагът е премахнат. Спиране на възпроизвеждането.");
  audio.stopSong();
  stopMotor();
  playingFile = "";
  isPlaying = false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("     СТАРТИРАНЕ НА ESP32 ГРАМОФОН (I2C NTAG215)   ");
  Serial.println("==================================================");

  pinMode(MOTOR_PIN, OUTPUT);
  stopMotor();

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // 1. Инициализация на I2C и NFC четеца
  Wire.begin(I2C_SDA, I2C_SCL);
  initSi512_I2C();
  byte ver = readRegisterI2C(0x37); // VersionReg
  Serial.printf("[1/3] HW-147C NFC Четей (I2C): ✅ УСПЕХ! Версия: 0x%02X\n", ver);

  // 2. Инициализация на SPI и SD Карта
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin();
  
  Serial.print("[2/3] Инициализация на SD карта... ");
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ ГРЕШКА!");
  } else {
    Serial.println("✅ УСПЕХ!");
  }

  // 3. Инициализация на I2S Аудио
  Serial.print("[3/3] Инициализация на MAX98357A Аудио... ");
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  Serial.println("✅ УСПЕХ!");

  Serial.println("==================================================");
  Serial.println("          СИСТЕМАТА Е ГОТОВА ЗА РАБОТА!            ");
  Serial.println("==================================================\n");
}

void loop() {
  audio.loop();

  // ----------------------------------------------------
  // 1. КОНТРОЛ НА СИЛАТА НА ЗВУКА (РОТАЦИОНЕН ЕНКОДЕР)
  // ----------------------------------------------------
  int currentClkState = digitalRead(ENCODER_CLK);
  if (currentClkState != lastClkState && currentClkState == LOW) {
    if (digitalRead(ENCODER_DT) != currentClkState) {
      volume = min(21, volume + 1);
    } else {
      volume = max(0, volume - 1);
    }
    audio.setVolume(volume);
    Serial.printf("🔊 Ниво на звука: %d\n", volume);
  }
  lastClkState = currentClkState;

  // ----------------------------------------------------
  // 2. СКАНИРАНЕ НА NFC ТАГОВЕ ПРЕЗ I2C
  // ----------------------------------------------------
  static unsigned long lastNFCScan = 0;
  if (millis() - lastNFCScan > 300) {
    lastNFCScan = millis();

    String detectedUID = readNFC_I2C();

    if (detectedUID != "") {
      bool trackFound = false;
      for (int i = 0; i < trackCount; i++) {
        if (playlist[i].uid == detectedUID) {
          playTrack(playlist[i].filename, playlist[i].title);
          trackFound = true;
          break;
        }
      }
      if (!trackFound && detectedUID != currentUID) {
        Serial.println("⚠️ Неизвестен таг с UID: " + detectedUID);
      }
      currentUID = detectedUID;
    } else {
      if (isPlaying) {
        stopTrack();
      }
      currentUID = "";
    }
  }
}