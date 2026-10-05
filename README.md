# NFC Vinyl Player

Battery-powered classic ESP32 NFC MP3 player. This folder is
the new standalone project and the root to upload to GitHub. No repository was
initialized and no hardware was flashed. Files beside this folder are historical
references; the older monolithic sketch and KiCad files are not the build inputs.

## Detected ESP32

User-reported detection output:

| Property | Reported value |
|---|---|
| Chip | ESP32-D0WD-V3 |
| Revision | v3.1 |
| Features | Wi-Fi, Bluetooth, dual core + low-power coprocessor, 240 MHz |
| eFuse | Vref calibration; coding scheme None |
| Crystal | 40 MHz |
| MAC | `30:76:f5:a9:dc:78` |

This confirms classic ESP32 silicon, not the development-board/module model,
flash size or PSRAM configuration. The wiring/build baseline still assumes a
DOIT ESP32 DEVKIT V1 with a WROOM module without PSRAM. You recall no PSRAM on an
earlier check, but no identifying module marking is available. GPIO16/17 must be
free of PSRAM use; see [PINOUT.md](PINOUT.md). The chip report alone does not
require a firmware or pin-map change.

## Start here

1. Use [PINOUT.md](PINOUT.md) for the single GPIO table and connection overview.
   Read [WIRING.md](WIRING.md) before soldering for power distribution, protection,
   through-hole parts, amplifier buffer and motor gate driver.
2. Shop from [BOM.csv](BOM.csv). `buy`, `keep`, `verify`, `select` and `fallback`
   are different decisions; do not buy an arbitrary protection board/fuse rating.
3. Follow [POWER_SEQUENCE.md](POWER_SEQUENCE.md) for cold-start versus powered
   sleep, including the timing limits. Use [MANUAL_TESTS.md](MANUAL_TESTS.md) for
   all manual hardware tests. Motor/audio require an NFC tag.
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

The supplied tag UIDs and assigned songs are listed in the rendered
[NFC tag table](NFC_TAGS.md). Use each UID exactly as shown for the MP3 filename.

## No-tag idle and USB

After successful initialization with no tag, motor PWM stays at zero and the
amplifier stays in shutdown. The LED ring shows its dim standby animation, NFC
polling continues, and KEY keep-alive requests continue every 20 s while awake.
This is lower-load operation, not zero current or disconnected peripheral power.
After five minutes without activity, the firmware sleeps and stops keep-alive;
whether IP5310 subsequently shuts down depends on actual standby current.

The firmware has no separate USB mode that disables idle sleep. **No tag does
not make simultaneous boost VIN and DevKit USB safe.** For programming, disconnect
the complete player harness, including GPIOs, and use DevKit USB alone. With SD
and PN532 disconnected the player may report an initialization fault; that is
not normal ready/idle operation, but it does not prevent programming.

K648 charging USB is a different power path from DevKit programming USB. Charging
while playing, isolated programming while charging, and fully wired dual-USB use
have separate tests and prerequisites in
[MANUAL_TESTS.md](MANUAL_TESTS.md#later-usb-and-charging-tests).

## Pinout

The checked table and compact schematic live in [PINOUT.md](PINOUT.md), including
the buffered LED/amplifier routes, motor driver, KEY transistor and boot-pin
conditions. It is checked against [firmware/nfc_player/pins.h](firmware/nfc_player/pins.h)
and [hardware.json](hardware.json); edit all three when changing GPIO assignments.

## Build and checks

Reference stack is pinned in [arduino-build.json](arduino-build.json). Core 2.0.17
is the initial full-build target. The PWM wrapper accommodates 3.x, but that does
not certify every dependency on 3.x. On a new Windows PC, install Arduino IDE
(the script finds its bundled CLI) or install `arduino-cli`, clone this repository,
open PowerShell in the repository root, then run:

```powershell
.\scripts\Build-Arduino.ps1 -Install
```

`-Install` reads `arduino-build.json` and installs the pinned ESP32 core and
libraries before compiling. The core is installed in Arduino CLI's normal
platform data directory; libraries are installed in this repository's ignored
`libraries/` folder, and compilation explicitly uses that folder. This keeps the
project dependencies together without committing downloaded library files. The
install needs internet access the first time. To run the checks separately, use:

```powershell
./scripts/Check-Design.ps1
./scripts/Test-DesignChecks.ps1
./scripts/Test-Control.ps1
```

Native tests require Visual Studio C++ tools and Windows SDK. Without `-Install`,
the build script only compiles and expects the pinned dependencies to already
exist in the repository's `libraries/` folder. Neither build command uploads.

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