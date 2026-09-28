#include "nfc.h"
#include "pins.h"
#include <stdio.h>
#include <string.h>

NfcReader::NfcReader() : device_(Pins::PN532_SCK, Pins::PN532_MISO, Pins::PN532_MOSI, Pins::PN532_SS) {}

bool NfcReader::begin() {
  if (!device_.begin() || !device_.getFirmwareVersion() || !device_.SAMConfig() ||
      !device_.setPassiveActivationRetries(Config::NFC_RETRY_COUNT)) return false;
  queue_ = xQueueCreate(1, sizeof(TagState));
  if (queue_ == nullptr) return false;
  stopRequested_.store(false);
  if (xTaskCreatePinnedToCore(taskEntry, "NFC", Config::NFC_TASK_STACK_BYTES, this, 1, nullptr, 0) != pdPASS) {
    vQueueDelete(queue_);
    queue_ = nullptr;
    return false;
  }
  return true;
}

bool NfcReader::poll(TagState &tag) { return queue_ != nullptr && xQueueReceive(queue_, &tag, 0) == pdTRUE; }
void NfcReader::stop() { stopRequested_.store(true); }
void NfcReader::taskEntry(void *instance) { static_cast<NfcReader *>(instance)->scan(); }

bool NfcReader::read(TagState &tag) {
  uint8_t bytes[10] = {}, length = 0;
  if (!device_.readPassiveTargetID(PN532_MIFARE_ISO14443A, bytes, &length, Config::NFC_READ_TIMEOUT_MS) ||
      length == 0 || length * 2U + 1U > sizeof(tag.uid)) return false;
  for (uint8_t index = 0; index < length; ++index) {
    snprintf(tag.uid + index * 2U, sizeof(tag.uid) - index * 2U, "%02X", bytes[index]);
  }
  return true;
}

void NfcReader::scan() {
  TagState current;
  uint8_t misses = 0;
  while (!stopRequested_.load()) {
    TagState detected;
    if (read(detected)) {
      misses = 0;
      if (strcmp(current.uid, detected.uid) != 0) {
        current = detected;
        xQueueOverwrite(queue_, &current);
      }
    } else if (current.uid[0] != '\0' && ++misses >= Config::NFC_MISSING_READS) {
      current = TagState{};
      misses = 0;
      xQueueOverwrite(queue_, &current);
    }
    vTaskDelay(pdMS_TO_TICKS(Config::NFC_SCAN_MS));
  }
  vTaskDelete(nullptr);
}