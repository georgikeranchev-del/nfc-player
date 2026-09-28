# Pinout and Connection Overview

This is the single GPIO connection table for the current battery-only build.
GPIO numbers are ESP32 signal names, NOT physical header positions; clone-board
layouts vary. Classic ESP32/WROOM without PSRAM remains the working assumption,
not a verified module identity. GPIO16/17 must be available, not used by PSRAM.

The symbol, GPIO and component columns are checked against
[firmware/nfc_player/pins.h](firmware/nfc_player/pins.h) and
[hardware.json](hardware.json). Update all three when deliberately changing the
pin map. Connection notes and the diagram are not an electrically checked netlist.
Component IDs correspond to [BOM.csv](BOM.csv).

<!-- PINOUT:BEGIN -->
| Symbol | GPIO | Component | Connection and conditions |
|---|---|---|---|
| AMP_ENABLE | 2 | U_BUFFER | SN74AHCT125N 2A, pin 5, with 10k to GND; 2Y, pin 6, goes to amplifier SD/SD_MODE. Never connect GPIO2 directly to amplifier SD. Boot-strap review required. |
| ENCODER_SW | 4 | ENC1 | Encoder push switch to GND; active LOW. Wakes a powered sleeping ESP32, not a shut-down IP5310. |
| PN532_SS | 5 | U_NFC | PN532 SS/SSEL; set the module to SPI. Verify reset-state module pulls on this boot-strap pin. |
| SD_CS | 13 | U_SD | SD module CS. |
| SD_SCK | 14 | U_SD | SD module SCK/CLK. |
| SD_MOSI | 15 | U_SD | SD module MOSI/DI. Verify reset-state pulls on this boot-strap pin. |
| SD_MISO | 16 | U_SD | SD module MISO/DO; GPIO16 must not be occupied by PSRAM. |
| IP5310_KEY | 17 | Q_KEY | 1k to 2N3904 base; 10k base-to-emitter; emitter to system GND; collector to IP5310 KEY. Never connect KEY directly to a GPIO. |
| PN532_SCK | 18 | U_NFC | PN532 SCK. |
| PN532_MISO | 19 | U_NFC | PN532 MISO. |
| LED_DATA | 21 | U_BUFFER | SN74AHCT125N 1A, pin 2, with 10k to GND; 1Y, pin 3, through 330 ohm to ring DIN. |
| I2S_DOUT | 22 | U_AMP | MAX98357A DIN. |
| PN532_MOSI | 23 | U_NFC | PN532 MOSI. |
| I2S_LRC | 25 | U_AMP | MAX98357A LRC/WS. |
| I2S_BCLK | 26 | U_AMP | MAX98357A BCLK. |
| MOTOR_GATE | 27 | U_GATE | TC4420 INPUT with 10k to GND; OUTPUT through 100 ohm to IRLZ44N gate. Keep 10k directly between gate and source. GPIO27 does not drive the MOSFET gate directly. |
| ENCODER_CLK | 32 | ENC1 | Encoder A/CLK; common to GND. |
| ENCODER_DT | 33 | ENC1 | Encoder B/DT; common to GND. |
<!-- PINOUT:END -->

All ESP32-side logic is **3.3 V, not 5 V tolerant**. GPIO12 and flash GPIO6-11 are
forbidden in this design. The retained straps 2, 5 and 15 are reviewed in
[DESIGN_DECISIONS.md](DESIGN_DECISIONS.md#boot-pin-review).

## Small connection schematic

```text
1S cell + fuse + protection --> IP5310/K648
  IP5310 OUT+ -- SW_LOAD --> LOAD_5V
  IP5310 OUT- -----------> LOAD_GND (protected system return)

LOAD_5V --+--> ESP32 VIN/5V
          +--> amplifier VIN, ring 5V, AHCT125 VCC, TC4420 VDD
          +--> motor +
motor - ------> IRLZ44N drain; source --> LOAD_GND
flyback diode: anode --> motor -; band/cathode --> motor +

GPIO27 --> TC4420 --> 100 ohm --> IRLZ44N gate
GPIO21 --> AHCT125 channel 1 --> 330 ohm --> ring DIN
GPIO2  --> AHCT125 channel 2 -------------> amplifier SD
GPIO17 --> 1k --> KEY transistor base; collector --> IP5310 KEY
GPIO4  --> encoder push switch --> LOAD_GND
IP5310 KEY --> separate NO contact/service connector --> LOAD_GND

LOAD_GND --> module grounds, buffer/driver grounds, KEY emitter
ESP32 3V3 --> encoder breakout VCC, only if it needs power
```

- SD and PN532 supply voltages depend on the actual breakout schematic; they are
  deliberately absent from the blanket 5 V list. Their SPI signals must be 3.3 V
  compatible. Do not drive any unpowered module through its GPIO connections.
- SN74AHCT125N DIP-14: VCC pin 14 to LOAD_5V, GND pin 7 to LOAD_GND, /OE pins 1
  and 4 to GND. Unused /OE pins 10/13 go to VCC, inputs 9/12 to GND; outputs 8/11
  stay unconnected. Add 100 nF at the supply pins. Use AHCT, not plain HC.
- TC4420 requires 100 nF plus 1 uF local bypass and all required GND/output pins
  wired per its exact package datasheet. Gate, driver-input and buffer-input
  pulldowns in the table remain required; this overview omits their drawn symbols.
- Amplifier SD must not be hard-strapped to VIN. GAIN is separate; both speaker
  wires go to amplifier outputs, neither to ground. See the breakout checks in
  [WIRING.md](WIRING.md#dip-ahct125-led-and-amplifier).
- The KEY transistor cannot cold-start an unpowered ESP32. Retain an already
  verified 2N7000 implementation instead if fitted; do not build both alternatives.

The diagram is an overview, not a replacement for protection terminals, capacitor
placement and reset-state checks in [WIRING.md](WIRING.md). All load returns must
remain on the protected side of the battery circuit.

## USB programming boundary

No tag is NOT power isolation. For the current programming procedure, disconnect
the complete player harness, including GPIOs, from ESP32 and use its USB alone.
Remove USB before reconnecting battery wiring. A powered ESP32 can back-feed
unpowered peripherals even with the motor stopped. Simultaneous charging and
fully wired dual-USB operation have separate gates in
[MANUAL_TESTS.md](MANUAL_TESTS.md#later-usb-and-charging-tests).