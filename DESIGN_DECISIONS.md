# Design Decisions

## Scope and canonical project

Keep the existing IP5310/K648, 6 V/50 RPM N20 and ESP32 DevKit V1. Battery-only
operation uses a TPS61088 JXL V1.10 set to 5 V, replacing XL6009, and a matched
VTC5A 1S2P pack. IP5310 is charger-only with OUT unused. USB charging/programming handover and shared
USB power are deliberately not part of the solder-ready baseline. The archived
monolith and old KiCad diagrams are not maintained build inputs.

## Power and reset

- Keep brownout enabled. A stable rising 5 V supply normally boots the DevKit
  through its regulator and reset circuit. Merely seeing 5 V once on a meter
  does not prove acceptable rise time, no dip or no overshoot.
- No mandatory series 1N5822 in the battery-only 5 V distribution: it wastes
  voltage headroom and does not provide complete two-source isolation.
- SW_LOAD switches protected P+ to both TPS61088 IN+ and the ADC divider top.
  IP5310 BAT stays connected upstream for charging while off. An output-only
  switch is unsuitable with a permanently connected battery ADC divider because
  it can back-power the ESP32. This is not an all-source USB cutoff.
- No KEY pulses or KEY driver are used. No charger/load mux is implemented.
- Deep sleep leaves boards and the converter connected. Battery monitoring
  stops in sleep; firmware is not a replacement for a PCM or hardware disconnect.
- Software stages initialization/actuation, not physical power to every board.
  True per-board rail sequencing requires load switches, not extra delays.

## One visible control

The current encoder remains connected to GPIO4/GND, active LOW. It handles
volume, normal powered-sleep wake and long-press sleep. GPIO17 and the former KEY
driver are unused. Cold-start is by SW_LOAD. Low-battery sleep deliberately has
no button/timer wake: switch off, recharge, then switch on. A single encoder does
not provide a physical battery disconnect; keep the DC-rated switch accessible.

For mechanical quality, consider a panel-supported Bourns PEC11R or Alps Alpine
EC11E variant WITH momentary push. Confirm shaft type/length, mounting, detent/
pulse counts, pinout and switch poles from the exact part datasheet. It improves
mechanical durability, not the power-disconnect requirement. Keep your working
encoder unless its contacts/mounting are a real issue; no new encoder is required.

## Logic and motor drive

- SN74AHCT125N DIP-14 serves both LED data and amp SD. It accepts 3.3 V logic with
  a 5 V supply and isolates the ESP32 from a breakout's 5 V SD pull-up. Channel 2
  input has a 10k pulldown so amp shutdown is the default during reset/sleep.
  GPIO2 no longer connects directly to amp SD. Use AHCT, not a plain HC125.
- Retain IRLZ44N but use TC4420CPA DIP-8 powered by LOAD_5V. This is a non-inverting
  gate driver with TTL-compatible input and a specified minimum supply of 4.5 V.
  It removes dependence on unspecified 3.3 V IRLZ44N on-resistance. Verify the
  exact part/datasheet and 5 V rail before assembly. Firmware polarity is unchanged.
- Keep 100 ohm gate resistor and 10k gate-to-source pulldown. Add 10k at the gate
  driver's input, and local bypass. The driver does NOT current-limit the motor.
- Use a Schottky flyback: baseline 1N5822 through-hole is a conservative 3 A
  candidate, subject to actual motor/transient/thermal ratings. 1N5819 is suitable
  only if measured motor ratings fit. Do not use 1N4007 at continuous 20 kHz PWM.
- Battery PCM/fuse selection requires cell and current data. A 3 A battery fuse
  is not equivalent to a 3 A 5 V output. Neither PCM nor a fuse stabilizes the rail.

## Boot-pin review

| GPIO | Reason retained | Required condition |
|---|---|---|
| 2 | Existing amp enable assignment | Buffer input only, 10k to GND; no amp VIN pull-up reaches GPIO2; verify serial boot. |
| 5 | Existing PN532 SS | Module must not force an incompatible reset strap level; verify breakout pull resistors. |
| 15 | Existing SD MOSI | Verify SD module reset-state pulls and 3.3 V signal levels; a LOW strap can suppress boot messages. |

GPIO12 is prohibited to avoid the flash-voltage strap hazard. Its former use was
unnecessary, not proof that every active pulse survives reset. GPIO16/17 are not
available on some PSRAM modules; "DevKit V1" clone branding alone is insufficient.

## Software boundaries

`Player` owns boot/play/sleep state and uses injected interfaces. Adapters own
Arduino/FreeRTOS APIs. `MotorControl` also injects a PWM port. The NFC task alone
owns PN532 reads and communicates via a one-slot latest-state queue. Its stop
request is atomic; no `volatile`-as-synchronization pattern is used.

PowerControl reads calibrated ADC millivolts through a 100k/47k divider on ADC1
GPIO34; a 100 nF filter and 16-sample average reduce noise. Player handles startup
voltage checks and sustained/critical low-voltage shutdown. Thresholds and meter
calibration are in config.h; 3.6 V nominal is not a calibration reference or an
accurate charge-percentage mapping. The decoder/SD/NFC calls can still block,
so sampling is not a hard real-time safety mechanism. Failures leave loads
off and report an error; startup does not continue with missing prerequisites.

Two-second startup is a target to measure, not a promise. Playback requires a
tag, and the software must never spin the motor merely because it powered up.
No scheduler tricks, networking, or permanent dummy load are added to conceal
power faults. Existing patterns, volume curve and three-play behavior are kept.