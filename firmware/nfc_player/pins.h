#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Pins {
constexpr uint8_t SD_CS = 13;
constexpr uint8_t SD_SCK = 14;
constexpr uint8_t SD_MISO = 16;
constexpr uint8_t SD_MOSI = 15;
constexpr uint8_t PN532_SS = 5;
constexpr uint8_t PN532_SCK = 18;
constexpr uint8_t PN532_MISO = 19;
constexpr uint8_t PN532_MOSI = 23;
constexpr uint8_t MOTOR_GATE = 27;
constexpr uint8_t I2S_LRC = 25;
constexpr uint8_t I2S_BCLK = 26;
constexpr uint8_t I2S_DOUT = 22;
constexpr uint8_t ENCODER_CLK = 32;
constexpr uint8_t ENCODER_DT = 33;
constexpr uint8_t ENCODER_SW = 4;
constexpr uint8_t LED_DATA = 21;
constexpr uint8_t AMP_ENABLE = 2;
constexpr uint8_t IP5310_KEY = 17;

constexpr uint8_t All[] = {
  SD_CS, SD_SCK, SD_MISO, SD_MOSI, PN532_SS, PN532_SCK, PN532_MISO,
  PN532_MOSI, MOTOR_GATE, I2S_LRC, I2S_BCLK, I2S_DOUT, ENCODER_CLK,
  ENCODER_DT, ENCODER_SW, LED_DATA, AMP_ENABLE, IP5310_KEY
};
constexpr size_t Count = sizeof(All) / sizeof(All[0]);
constexpr bool unique(size_t first = 0, size_t next = 1) {
  return first >= Count ? true : next >= Count ? unique(first + 1, first + 2)
    : All[first] != All[next] && unique(first, next + 1);
}
constexpr bool allowed(size_t index = 0) {
  return index == Count ? true
    : (All[index] == 0 || All[index] == 2 || All[index] == 4 || All[index] == 5 ||
       (All[index] >= 13 && All[index] <= 19) || (All[index] >= 21 && All[index] <= 23) ||
       (All[index] >= 25 && All[index] <= 27) || All[index] == 32 || All[index] == 33)
      && allowed(index + 1);
}
static_assert(unique(), "Every component must have its own GPIO");
static_assert(allowed(), "Unsupported GPIO or reserved GPIO12/flash/UART/input-only pin");
static_assert(ENCODER_SW == 4, "Changing wake GPIO requires reviewing RTC wake wiring");
}