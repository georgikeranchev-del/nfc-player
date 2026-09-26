#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// Дефиниране на пиновете за SPI
#define PN532_SCK  (18)
#define PN532_MISO (19)
#define PN532_MOSI (23)
#define PN532_SS   (5)

// Инициализиране на PN532 обекта през Хардуерен SPI
Adafruit_PN532 nfc(PN532_SS);

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); // Чакане на Serial порта

  Serial.println("\n--- Стартиране на тест за PN532 през SPI ---");

  nfc.begin();

  // Проверка дали PN532 е свързан правилно
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("ГРЕШКА: Не е намерен PN532 борд! Провери окабеляването и DIP ключетата.");
    while (1); // Спира тук при грешка
  }
  
  // При успешна връзка изписва версията на чипа
  Serial.print("Намерен чип PN532 с версия на фърмуера: ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); 
  Serial.println((versiondata >> 8) & 0xFF, DEC);
  
  // Конфигуриране на борда да чете RFID/NFC тагове
  nfc.SAMConfig();
  
  Serial.println("PN532 е готов! Доближи NFC таг (NTAG215 стикер или карта)...");
}

void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Буфер за съхранение на UID (до 7 байта)
  uint8_t uidLength;                        // Дължина на UID (4 или 7 байта)
    
  // Опит за четене на ISO14443A таг (NTAG215, MIFARE и др.)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500);
  
  if (success) {
    // Намерен е таг!
    Serial.println("\n----------------------------------");
    Serial.println("УСПЕХ: Намерен е NFC таг!");
    
    Serial.print("Дължина на UID: ");
    Serial.print(uidLength, DEC);
    Serial.println(" байта");
    
    Serial.print("UID (HEX): ");
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) Serial.print(" 0");
      else Serial.print(" ");
      Serial.print(uid[i], HEX);
    }
    Serial.println();
    
    // Бърза проверка какъв тип е тагът според дължината на UID
    if (uidLength == 7) {
      Serial.println("Тип на тага: NTAG / MIFARE Ultralight (Стандартен за грамофонни плочи)");
    } else if (uidLength == 4) {
      Serial.println("Тип на тага: MIFARE Classic / 1K");
    }

    // Изчакване 1 секунда, за да не чете повторно мигновено
    delay(1000);
  }
}