#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <MFRC522.h>
#include "Audio.h"

// ==========================================
// 1. ДЕФИНИЦИЯ НА ПИНОВЕ
// ==========================================
#define SD_CS          13
#define MFRC_SS        5
#define MFRC_RST       4
#define MOTOR_PIN      27

#define I2S_LRC        25
#define I2S_BCLK       26
#define I2S_DOUT       22

#define ENCODER_CLK    32
#define ENCODER_DT     33
#define ENCODER_SW     34

const int MAX_LOOPS = 3;

MFRC522 mfrc522(MFRC_SS, MFRC_RST);
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
  {"04E76391E52A81", "/player/08.mp3"},
  {"04EB6391E52A81", "/player/09.mp3"},
  {"04915D91E52A81", "/player/10.mp3"},
  {"04965D91E52A81", "/player/11.mp3"},
  {"047C5D91E52A81", "/player/12.mp3"},
  {"04845D91E52A81", "/player/13.mp3"},
  {"04235891E52A81", "/player/14.mp3"},
  {"04245891E52A81", "/player/15.mp3"},
  {"04EB5791E52A81", "/player/16.mp3"},
  {"04F15791E52A81", "/player/17.mp3"},
  {"04845291E52A81", "/player/18.mp3"},
  {"04895291E52A81", "/player/19.mp3"},
  {"045C5291E52A81", "/player/20.mp3"},
  {"04645291E52A81", "/player/21.mp3"},
  {"04E24B91E52A81", "/player/22.mp3"},
  {"04E34B91E52A81", "/player/23.mp3"},
  {"04A84B91E52A81", "/player/24.mp3"},
  {"04AD4B91E52A81", "/player/25.mp3"},
  {"042F4691E52A81", "/player/26.mp3"},
  {"04354691E52A81", "/player/27.mp3"},
  {"04E84591E52A81", "/player/28.mp3"},
  {"04EF4591E52A81", "/player/29.mp3"},
  {"046C3F91E52A81", "/player/30.mp3"},
  {"046D3F91E52A81", "/player/31.mp3"},
  {"04283F91E52A81", "/player/32.mp3"},
  {"042F3F91E52A81", "/player/33.mp3"},
  {"048F3991E52A81", "/player/34.mp3"},
  {"04953991E52A81", "/player/35.mp3"},
  {"04483991E52A81", "/player/36.mp3"},
  {"044B3991E52A81", "/player/37.mp3"},
  {"04AF3391E52A81", "/player/38.mp3"},
  {"04B33391E52A81", "/player/39.mp3"},
  {"04643391E52A81", "/player/40.mp3"},
  {"04693391E52A81", "/player/41.mp3"},
  {"047E2E91E52A81", "/player/42.mp3"},
  {"04862E91E52A81", "/player/43.mp3"},
  {"04392E91E52A81", "/player/44.mp3"},
  {"043E2E91E52A81", "/player/45.mp3"},
  {"04192991E52A81", "/player/46.mp3"},
  {"041E2991E52A81", "/player/47.mp3"},
  {"04A6B591E52A81", "/player/48.mp3"},
  {"04B72891E52A81", "/player/49.mp3"},
  {"04192391E52A81", "/player/50.mp3"}
};
const int trackCount = sizeof(playlist) / sizeof(playlist[0]);

String currentUID = "";
String playingFile = "";
bool isPlaying = false;
int loopCount = 0;
int volume = 12;
int lastClkState;

void startMotor() { digitalWrite(MOTOR_PIN, HIGH); }
void stopMotor()  { digitalWrite(MOTOR_PIN, LOW); }

String formatUIDWithColons(String rawUID) {
  String formatted = "";
  for (int i = 0; i < rawUID.length(); i += 2) {
    if (i > 0) formatted += ":";
    formatted += rawUID.substring(i, i + 2);
  }
  return formatted;
}

// Избиране на HW-147C на SPI шината
void selectNFC() {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(MFRC_SS, LOW);
}

// Освобождаване на SPI шината
void deselectNFC() {
  digitalWrite(MFRC_SS, HIGH);
}

String readNFCUID() {
  selectNFC();
  
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    deselectNFC();
    return "";
  }

  String cardUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  deselectNFC();

  return cardUID;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("   СТАРТИРАНЕ НА ДИАГНОСТИКА НА ESP32 ГРАМОФОН    ");
  Serial.println("==================================================");

  pinMode(SD_CS, OUTPUT);
  pinMode(MFRC_SS, OUTPUT);
  pinMode(MFRC_RST, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(MFRC_SS, HIGH);

  // Хардуерен рестарт на HW-147C
  digitalWrite(MFRC_RST, LOW);
  delay(50);
  digitalWrite(MFRC_RST, HIGH);
  delay(50);

  pinMode(MOTOR_PIN, OUTPUT);
  stopMotor();
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  SPI.begin();

  // 1. Проверка SD Карта
  Serial.print("[1/3] Проверка на MicroSD картата... ");
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ ГРЕШКА!");
  } else {
    Serial.println("✅ УСПЕХ!");
  }

  // 2. Проверка HW-147C
  Serial.print("[2/3] Проверка на HW-147C NFC четеца... ");
  selectNFC();
  mfrc522.PCD_Init();
  delay(100);

  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  deselectNFC();

  if (version == 0x00 || version == 0xFF) {
    Serial.println("❌ ГРЕШКА! Чипът не отговаря!");
  } else {
    Serial.printf("✅ УСПЕХ! Регистър: 0x%02X\n", version);
  }

  // 3. MAX98357A Аудио
  Serial.print("[3/3] Инициализация на MAX98357A Аудио... ");
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  Serial.println("✅ УСПЕХ!");

  Serial.println("==================================================\n");
}

void loop() {
  audio.loop();

  // Ротационен енкодер
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

  // NFC Проверка
  static unsigned long lastNFCScan = 0;
  if (millis() - lastNFCScan > 300) {
    lastNFCScan = millis();
    String detectedUID = readNFCUID();

    if (detectedUID != "") {
      Serial.println("--------------------------------------------------");
      Serial.println("🏷️  СКАНИРАН ТАГ: " + formatUIDWithColons(detectedUID));
      Serial.println("--------------------------------------------------");
    }
  }
}