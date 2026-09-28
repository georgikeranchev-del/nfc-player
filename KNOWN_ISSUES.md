# Known Issues and Verification Gates

| Item | Status / next check |
|---|---|
| Real ESP32 compile | Blocked locally: board core absent, Arduino DNS downloads fail. CI is prepared but has not run here. |
| Native coverage | Controller/motor tests use fake ports; they do not prove ESP32 APIs, task timing, I2S audio or electrical behavior. |
| Board identity | Check WROOM marking, regulator and expansion-board routing. GPIO16/17 conflict with PSRAM on other variants. |
| IP5310 power handover | Battery-only baseline; charging while playing is deferred until module pass-through behavior is measured. |
| Cold boot vs sleep | KEY restores a shut-down rail; it is not an ESP32 wake input when the rail stays on. |
| Single encoder cold-start | Not implemented electrically. Keep GPIO4 independent and a KEY service connector until thresholds/gestures are measured. |
| Deep sleep current | All supply branches remain powered; auto-off is not guaranteed, especially with buffer/gate driver/PN532 standby load. |
| KEY pulse accuracy | ESP timer task can be delayed; measure. Firmware cannot enforce IP5310 behavior or wake itself from no power. |
| Boot timing | Stages are minimum offsets; SD/PN532 and decoder calls can block. Two seconds is not a guaranteed deadline. |
| 3.x support | PWM wrapper has both APIs; reference full stack is core 2.0.17. Don't infer full 3.x audio compatibility. |
| NFC presence queue | Rapid transitions can coalesce; a remove/re-present entirely between consumption can be missed. Reader detection/retries need bench checks. |
| MP3 errors | Decoder false means finished/error; bounded replay can repeat a corrupt track up to MAX_PLAYS. Fix bad files, not endless retries. |
| UID size | Supports up to 7 UID bytes/14 hex chars, preserving original behavior; 10-byte UIDs rejected. |
| Encoder fast turns | Polling can miss fast detents during library calls. No quadrature ISR decoder added without a demonstrated need. |
| Amp channel | SD selects shutdown/channel, GAIN selects gain. HIGH SD selects a channel; test stereo content, use mono files as appropriate. |
| Protection | Cell discharge/charge rating and PCM/fuse sizing remain unverified. No fixed 3 A promise. |
| Motor current | PWM ramp/TC4420 are not current limiting. Stall, belt load, flyback rating and temperature need measurements. |
| Digital checks | Validate declarations/BOM consistency only, not physical solder joints or KiCad electrical correctness. |
| Historical files | Parent monolith, wiring guide and KiCad files are archived references. Only this folder is the modular project. |

No simultaneous external VIN and DevKit USB until both power paths are verified.
For uploads, disconnect the player harness from the DevKit to avoid back-powering
unpowered peripherals via GPIOs. The battery-only baseline intentionally omits
a series supply diode; it is not safe to add USB just because the diode is absent.

Do not resume testing the boost that previously produced 8-9 V. Inspect possibly
damaged components and use current-limited power during staged bring-up.