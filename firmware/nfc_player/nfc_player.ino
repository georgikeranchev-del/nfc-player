#include "player.h"
#include "power.h"
#include "motor.h"
#include "leds.h"
#include "audio.h"
#include "nfc.h"
#include "storage.h"
#include "encoder.h"

PowerControl Power;
Esp32Pwm MotorPwm;
MotorControl Motor(MotorPwm);
LedRing Leds;
AudioPlayer Audio;
NfcReader Nfc;
SdStorage Storage;
EncoderInput Encoder;
Player Application(Power, Motor, Leds, Audio, Nfc, Storage, Encoder);

void setup() { Application.begin(millis()); }

void loop() {
  Application.tick(millis());
  static PlayerState lastState = PlayerState::Sleeping;
  static const char *lastError = "";
  if (Application.state() != lastState) {
    lastState = Application.state();
    Serial.printf("Player state: %u at %lu ms\n", static_cast<unsigned>(lastState),
                  static_cast<unsigned long>(millis()));
  }
  if (Application.error() != lastError) {
    lastError = Application.error();
    if (lastError[0] != '\0') Serial.println(lastError);
  }
  delay(1);
}