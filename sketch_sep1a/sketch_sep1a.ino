#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Adafruit_PN532.h>
#include "Audio.h" // Библиотека: ESP32-audioI2S by schreibfaul1

// ==========================================
// 1. ДЕФИНИЦИЯ НА ПИНОВЕ И НАСТРОЙКИ
// ==========================================

// --- MicroSD Карта (HSPI Bus) ---
#define SD_CS   13
#define SD_SCK  14
#define SD_MISO 16
#define SD_MOSI 15

// --- PN532 NFC Четец ---
#define PN532_SS   5
#define PN532_SCK  18
#define PN532_MISO 19
#define PN532_MOSI 23

// --- Мотор ---
#define MOTOR_PIN 27

// --- I2S Аудио пинове за MAX98357A ---
#define I2S_LRC  25
#define I2S_BCLK 26
#define I2S_DOUT 22

// --- Ротационен енкодер KY-040 ---
#define ENCODER_CLK 32
#define ENCODER_DT  33
#define ENCODER_SW  4  // Сменено от 34 на 4 за ползване на вътрешен INPUT_PULLUP

const int MAX_LOOPS = 3; // Максимален брой повторения преди Deep Sleep

// ==========================================
// 2. ИНИЦИАЛИЗАЦИЯ НА ОБЕКТИ И ПРОМЕНЛИВИ
// ==========================================

// Независима SPI шина за SD картата
SPIClass sdSPI(HSPI);

// Software SPI за PN532 NFC (За да няма конфликт с SD картата)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Аудио обект
Audio audio

// Структура за връзка между NFC UID и MP3 файл
struct TrackMap {
  String uid;
  const char* filename;
};

// ТАБЛИЦА С ВАШИТЕ ТАГОВЕ И ПЕСНИ (Всички 50 песни)
TrackMap playlist[] = {
  {"047D5D91E52A81", "/player/1.mp3"},   // Mariah Carey - All I Want for Christmas Is You
  {"04955D91E52A81", "/player/2.mp3"},   // Wham! - Last Christmas
  {"04925D91E52A81", "/player/3.mp3"},   // Ariana Grande - Santa Tell Me
  {"04EA6391E52A81", "/player/4.mp3"},   // Bobby Helms - Jingle Bell Rock
  {"04E86391E52A81", "/player/5.mp3"},   // Andy Williams - It's the Most Wonderful Time of the Year
  {"04F16391E52A81", "/player/6.mp3"},   // Dean Martin - Let It Snow! Let It Snow! Let It Snow!
  {"04F26391E52A81", "/player/7.mp3"},
  {"04E76391E52A81", "/player/8.mp3"},
  {"04EB6391E52A81", "/player/9.mp3"},
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

// Логически променливи
String currentUID = "";
String playingFile = "";
bool isPlaying = false;
int loopCount = 0;

int volume = 12; // Начална сила на звука (скала 0 - 21)
int lastClkState;

// ==========================================
// 3. ПОМОЩНИ ФУНКЦИИ
// ==========================================
void startMotor() {
  digitalWrite(MOTOR_PIN, HIGH);
}

void stopMotor() {
  digitalWrite(MOTOR_PIN, LOW);
}

// Преминаване в дълбок сън при изчерпване на лимита
void enterDeepSleep() {
  Serial.println("\n[SLEEP] Достигнат е лимитът от 3 пускания. Влизане в Deep Sleep...");
  stopMotor();
  audio.stopSong();
  delay(100);
  esp_deep_sleep_start();
}

// Четене на UID от NFC тага
String readNFCUID() {
  uint8_t success;
  uint8_t uid[7];
  uint8_t uidLength;

  // Трайност на сканирането 50ms за да не насича аудиото
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);

  if (success) {
    String cardUID = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) cardUID += "0";
      cardUID += String(uid[i], HEX);
    }
    cardUID.toUpperCase();
    return cardUID;
  }
  return "";
}

// ==========================================
// 4. SETUP (ПЪРВОНАЧАЛНА ИНИЦИАЛИЗАЦИЯ)
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Стартиране на ESP32 Грамофон ===");

  // Настройка на пин за мотор
  pinMode(MOTOR_PIN, OUTPUT);
  stopMotor();

  // Настройка на пинове за енкодер
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // 1. Инициализация на MicroSD картата през отделна HSPI шина
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("[ГРЕШКА] MicroSD картата не е намерена!");
  } else {
    Serial.println("[OK] MicroSD картата е намерена.");
  }

  // 2. Инициализация на I2S Аудиото
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);

  // 3. Инициализация на PN532 NFC
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("[ГРЕШКА] PN532 NFC четецът не бе намерен!");
  } else {
    nfc.SAMConfig(); // Настройка за четене
    nfc.setPassiveActivationRetries(0x11); // Задължително за да не блокира при четене!
    Serial.println("[OK] PN532 NFC четецът е готов.");
  }
}

// ==========================================
// 5. ОСНОВЕН ЦИКЪЛ (LOOP)
// ==========================================
void loop() {
  // А. Задължително обновяване на аудио буфера
  audio.loop();

  // Б. Обработка на края на песента (Логика за Loop x3 -> Deep Sleep)
  if (isPlaying && !audio.isRunning()) {
    loopCount++;
    Serial.printf("[LOOP] Песента завърши (%d/%d повторения)\n", loopCount, MAX_LOOPS);

    if (loopCount < MAX_LOOPS) {
      // Повторно пускане на същата песен (без sdSPI като 3-ти параметър!)
      audio.connecttoFS(SD, playingFile.c_str());
    } else {
      // Достигнат е лимитът -> Заспиване
      enterDeepSleep();
    }
  }

  // В. Четене на Ротационен Енкодер (Управление на звука)
  int currentClkState = digitalRead(ENCODER_CLK);
  if (currentClkState != lastClkState && currentClkState == LOW) {
    if (digitalRead(ENCODER_DT) != currentClkState) {
      volume = min(21, volume + 1); // Увеличаване (макс 21)
    } else {
      volume = max(0, volume - 1);  // Намаляване (мин 0)
    }
    audio.setVolume(volume);
    Serial.printf("[VOLUME] Ниво на звука: %d\n", volume);
  }
  lastClkState = currentClkState;

  // Г. Периодично сканиране за NFC Таг (на всеки 300ms)
  static unsigned long lastNFCScan = 0;
  if (millis() - lastNFCScan > 300) {
    lastNFCScan = millis();
    String detectedUID = readNFCUID();

    // Случай 1: Поставена е нова плоча
    if (detectedUID != "" && detectedUID != currentUID) {
      currentUID = detectedUID;
      bool trackFound = false;

      for (int i = 0; i < trackCount; i++) {
        if (playlist[i].uid == currentUID) {
          playingFile = playlist[i].filename;
          loopCount = 0; // Нулиране на брояча за новата плоча!
          
          Serial.println("[PLAY] Намерена плоча! Стартиране: " + playingFile);
          startMotor();
          
          // Старт на възпроизвеждането (коригирано за библиотеката)
          isPlaying = audio.connecttoFS(SD, playingFile.c_str());
          trackFound = true;
          break;
        }
      }

      if (!trackFound) {
        Serial.println("[WARN] Непознат NFC таг с UID: " + currentUID);
      }
    }
    // Случай 2: Плочата е премахната
    else if (detectedUID == "" && currentUID != "") {
      Serial.println("[STOP] Плочата е вдигната. Спиране на възпроизвеждането...");
      audio.stopSong();
      stopMotor();
      currentUID = "";
      playingFile = "";
      isPlaying = false;
      loopCount = 0;
    }
  }
}