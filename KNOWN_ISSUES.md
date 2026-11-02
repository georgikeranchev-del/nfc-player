# Known Issues and Verification Gates

| Item | Status / next check |
|---|---|
| Real ESP32 compile | Not run for the battery-monitor change. Earlier local builds were blocked by missing core/downloads; native tests do not validate ADC/sleep APIs. |
| Native coverage | Controller/motor tests use fake ports; they do not prove ESP32 APIs, task timing, I2S audio or electrical behavior. |
| Board identity | Check WROOM marking, regulator and expansion-board routing. GPIO16/17 conflict with PSRAM on other variants. |
| TPS61088 JXL V1.10 | Set/verify 5.00 V before attaching loads. Actual board current, thermal, startup, UVLO and EN behavior remain unverified; EN is not controlled by firmware. |
| IP5310 charging | BAT port remains on protected pack; OUT unused. A load at the battery port can affect charging/termination; verify net charging and termination while playing. No automatic source mux. |
| Cold boot vs sleep | SW_LOAD powers boost and divider together; GPIO4 wakes normal sleep only. KEY driver and pulse timer removed. |
| Battery ADC | GPIO34 needs 100k/47k 1% divider plus 100nF, protected return and meter calibration. Wire it before using the firmware; missing/zero readings prevent startup. |
| Low-battery limits | Initial settings: boot >=3.50 V; <=3.30 V for 2 s stops playback; <=3.00 V stops on next sample. Validate against sag/ADC error; these are not cell absolute limits or charge percentages. |
| Deep sleep current | Converter/peripherals remain connected and ADC monitoring stops. Low-battery sleep disables all wake sources but is not isolation; switch off and recharge. Keep PCM protection and use a hardware disconnect if autonomous cutoff is required. |
| Boot timing | Stages are minimum offsets; SD/PN532 and decoder calls can block. Two seconds is not a guaranteed deadline. |
| 3.x support | PWM wrapper has both APIs; reference full stack is core 2.0.17. Don't infer full 3.x audio compatibility. |
| NFC presence queue | Rapid transitions can coalesce; a remove/re-present entirely between consumption can be missed. Reader detection/retries need bench checks. |
| MP3 errors | Decoder false means finished/error; bounded replay can repeat a corrupt track up to MAX_PLAYS. Fix bad files, not endless retries. |
| UID size | Supports up to 7 UID bytes/14 hex chars, preserving original behavior; 10-byte UIDs rejected. |
| Encoder fast turns | Polling can miss fast detents during library calls. No quadrature ISR decoder added without a demonstrated need. |
| Amp channel | SD selects shutdown/channel, GAIN selects gain. HIGH SD selects a channel; test stereo content, use mono files as appropriate. |
| Protection | Matched VTC5A 1S2P construction and total-current PCM/fuse sizing remain unverified. Pack voltage is still 4.20 V full; no fixed 3 A promise. |
| Motor current | PWM ramp/TC4420 are not current limiting. Stall, belt load, flyback rating and temperature need measurements. |
| Digital checks | Validate declarations/BOM consistency only, not physical solder joints or KiCad electrical correctness. |
| Historical files | Parent monolith, wiring guide and KiCad files are archived references. Only this folder is the modular project. |

No simultaneous external VIN and DevKit USB until both power paths are verified.
For uploads, disconnect the player harness from the DevKit to avoid back-powering
unpowered peripherals via GPIOs. The battery-only baseline intentionally omits
a series supply diode; it is not safe to add USB just because the diode is absent.

Do not resume testing the boost that previously produced 8-9 V. Inspect possibly
damaged components and use current-limited power during staged bring-up.