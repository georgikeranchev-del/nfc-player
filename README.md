# NFC Vinyl Player

Battery-powered ESP32 DevKit V1 / ESP32-WROOM-32 NFC MP3 player. This folder is
the new standalone project and the root to upload to GitHub. No repository was
initialized and no hardware was flashed. Files beside this folder are historical
references; the older monolithic sketch and KiCad files are not the build inputs.
Used ESP32:
Chip type:          ESP32-D0WD-V3 (revision v3.1)
Features:           Wi-Fi, BT, Dual Core + LP Core, 240MHz, Vref calibration in eFuse, Coding Scheme None
Crystal frequency:  40MHz
MAC:                30:76:f5:a9:dc:78

## Start here

1. Read [WIRING.md](WIRING.md) before soldering. It specifies the battery-only
   baseline, through-hole parts, amplifier buffer and motor gate driver.
2. Shop from [BOM.csv](BOM.csv). `buy`, `keep`, `verify`, `select` and `fallback`
   are different decisions; do not buy an arbitrary protection board/fuse rating.
3. Follow [POWER_SEQUENCE.md](POWER_SEQUENCE.md) for cold-start versus powered
   sleep, including the timing limits. Motor/audio require an NFC tag.
4. Review [KNOWN_ISSUES.md](KNOWN_ISSUES.md). This is not an electrically certified
   schematic. Do not order a PCB from the old KiCad files.

## Firmware

Open [firmware/nfc_player/nfc_player.ino](firmware/nfc_player/nfc_player.ino) in
Arduino IDE. Keep its sibling `.h` and `.cpp` files in the same sketch directory.
Do not copy the old monolithic sketch into this directory; it defines another
`setup()`/`loop()`. Select DOIT ESP32 DEVKIT V1 (classic WROOM, not S3/C3/WROVER).

- [config.h](firmware/nfc_player/config.h): all user tuning, volumes, timing,
  loop count, LED count/pattern mask and motor run duty.
- [pins.h](firmware/nfc_player/pins.h): the one GPIO map, with compile-time checks.
- [player.cpp](firmware/nfc_player/player.cpp): orchestration and sequencing.
- `power`, `motor`, `leds`, `audio`, `nfc`, `storage`, `encoder`: hardware adapters.
- [interfaces.h](firmware/nfc_player/interfaces.h): dependency-injection contracts.

The composition root creates objects and injects them into `Player`. Calls such
as `Motor.stop()` go through an `IMotor&`; native tests inject fakes. Merely using
object syntax would not be dependency injection. A separate `IPwm` lets the real
motor ramp algorithm run in tests without Arduino.

MP3 files belong at `/player/<UPPERCASE_UID>.mp3` on the SD card. Default behavior:
three plays, 600 ms motor ramp, 1 s audio fade, 5-minute idle sleep, 1.5 s encoder
hold then release to sleep. A held wake button must first be released to arm a
new long press. Brownout protection is NOT disabled.

## Pinout

This table is checked against `pins.h` and `hardware.json`. Edit all three when
deliberately changing wiring. Symbolic `constexpr` pins replace the old `#define`s.

<!-- PINOUT:BEGIN -->
| Symbol | GPIO | Component |
|---|---|---|
| SD_CS | 13 | U_SD |
| SD_SCK | 14 | U_SD |
| SD_MISO | 16 | U_SD |
| SD_MOSI | 15 | U_SD |
| PN532_SS | 5 | U_NFC |
| PN532_SCK | 18 | U_NFC |
| PN532_MISO | 19 | U_NFC |
| PN532_MOSI | 23 | U_NFC |
| MOTOR_GATE | 27 | U_GATE |
| I2S_LRC | 25 | U_AMP |
| I2S_BCLK | 26 | U_AMP |
| I2S_DOUT | 22 | U_AMP |
| ENCODER_CLK | 32 | ENC1 |
| ENCODER_DT | 33 | ENC1 |
| ENCODER_SW | 4 | ENC1 |
| LED_DATA | 21 | U_BUFFER |
| AMP_ENABLE | 2 | U_BUFFER |
| IP5310_KEY | 17 | Q_KEY |
<!-- PINOUT:END -->

GPIO12 and flash GPIO6-11 are forbidden. Existing straps 2, 5 and 15 are explicitly
reviewed in [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md); this is not a claim that
all boot pins are interchangeable. GPIO16/17 require a WROOM board without PSRAM.

## Build and checks

Reference stack is pinned in [arduino-build.json](arduino-build.json). Core 2.0.17
is the initial full-build target. The PWM wrapper accommodates 3.x, but that does
not certify every dependency on 3.x. From this project root in PowerShell:

```powershell
./scripts/Check-Design.ps1
./scripts/Test-DesignChecks.ps1
./scripts/Test-Control.ps1
./scripts/Build-Arduino.ps1 -Install
```

Native tests require Visual Studio C++ tools and Windows SDK. Arduino build
requires `arduino-cli` and network access on first install; the script also finds
the bundled Arduino IDE CLI on Windows. `-Install` explicitly installs pinned
dependencies. Without it, the script only compiles. Neither command uploads.

[GitHub Actions](.github/workflows/checks.yml) runs design rules, rule-mutation
tests, native control tests and a separate real Arduino compilation. Push the
CONTENTS of this directory as repository root; a workflow in an arbitrary nested
subdirectory of another repository is not discovered by GitHub.

Rules fail for GPIO12, duplicate GPIOs, missing strap review, pinout drift,
missing/extra BOM entries, missing baseline motor gate/source pulldown, or a
brownout-disable override. They validate declared design data, not actual wiring,
PCB netlists, transistor pin order, current ratings or safe battery construction.

Local verification: the controller passes native tests. A real ESP32 build has
not completed here: the platform is not installed and Arduino DNS/downloads fail.
Hardware adapters and power timing still need an actual board test.