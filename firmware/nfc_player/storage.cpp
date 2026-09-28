#include "storage.h"
#include "pins.h"
#include <SD.h>

bool SdStorage::begin() {
  spi_.begin(Pins::SD_SCK, Pins::SD_MISO, Pins::SD_MOSI, Pins::SD_CS);
  if (!SD.begin(Pins::SD_CS, spi_, Config::SD_SPEED_HZ)) return false;
  return SD.exists(Config::TRACK_DIRECTORY) || SD.mkdir(Config::TRACK_DIRECTORY);
}

bool SdStorage::exists(const char *path) { return SD.exists(path); }