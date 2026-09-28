#pragma once
#include "interfaces.h"
#include <SPI.h>

class SdStorage : public IStorage {
public:
  SdStorage() : spi_(HSPI) {}
  bool begin() override;
  bool exists(const char *path) override;
private:
  SPIClass spi_;
};