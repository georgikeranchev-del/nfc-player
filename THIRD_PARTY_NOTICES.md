# Third-party notices

## Scope

The `LICENSE` applies to original project code and documentation in this
repository branch. It does not replace or extend the licenses of third-party
software, hardware documentation, trademarks, or other separately published
material. Dependencies are downloaded during setup rather than committed in
this branch; their own notices and license terms continue to apply.

The versions below are pinned in [`arduino-build.json`](arduino-build.json).
Check the license files and notices from those exact upstream versions when
redistributing dependency source or a firmware binary.

| Component | Pinned version | License / notice |
|---|---:|---|
| ESP32 Arduino core | 2.0.17 | Arduino core headers identify LGPL-2.1-or-later; bundled components can have separate terms. Review notices for the exact core package. |
| Adafruit PN532 | 1.3.4 | BSD license, as identified by the upstream project; retain its required copyright, conditions, disclaimer, and attribution text. |
| Adafruit BusIO | 1.17.0 | MIT |
| FastLED | 3.9.15 | MIT |
| ESP8266Audio | 1.9.7 | Contains components with their own notices. The MP3 decoder used by this project identifies itself as GPL-3.0-or-later. Review notices for the exact package. |

Upstream project pages:

- [arduino-esp32](https://github.com/espressif/arduino-esp32)
- [Adafruit-PN532](https://github.com/adafruit/Adafruit-PN532)
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)
- [FastLED](https://github.com/FastLED/FastLED)
- [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)

## Firmware redistribution

The build links third-party libraries into the firmware. Before distributing a
compiled firmware binary, identify the licenses of the exact included
components, preserve and provide their required notices and license texts, and
meet the applicable source-code and other license obligations. In particular,
the GPL-covered MP3 decoder means recipients must be given the corresponding
source and license rights required by GPL-3.0-or-later. This repository's
firmware source and pinned dependency list are starting points, not by
themselves a complete compliance package for every binary build.

The song titles in [`NFC_TAGS.md`](NFC_TAGS.md) are identification references;
this repository branch does not include the recordings.
