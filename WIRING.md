# NFC MP3 Player — Wiring Reference

Companion to `nfc_player.cpp`, revised 2026-09-27. Keep the IP5310, N20 motor,
and the goal of one external encoder. This is a bench plan, not a verified PCB
schematic. Existing KiCad files have NOT been updated or electrically reviewed.

## Read before applying power

- Firmware no longer disables brownout protection. Leave the Arduino core's
  default protection enabled; resets are evidence to investigate, not suppress.
- Do not connect DevKit USB and external VIN power together until the exact
  DevKit/expansion-board power circuit is verified. One series diode is NOT
  complete two-source isolation. Never reconnect the boost that produced 8-9 V.
- Deep sleep does not disconnect peripheral power or guarantee IP5310 auto-off.
  A KEY tap restores a shut-down boost, but does not necessarily wake an ESP32
  that is still powered and asleep.
- Keep encoder SW on GPIO4/GND for now. Do not connect it directly to KEY, or
  build the previously proposed 47k/10k KEY-sensing circuit without measurements.
- Brownout protection is not battery protection. Use a documented protected
  cell or suitable 1S PCM, plus a correctly selected battery fuse.

## Firmware changes and limits

- KEY remains GPIO17. Pulses are requested every 20 s, including playback;
  quiet playback is not assumed to guarantee the bank's minimum load.
- An ESP timer releases KEY after nominally 120 ms, independently of loop().
  Timer-task scheduling still has jitter: measure the waveform on hardware.
- Motor ramp is non-blocking, 600 ms to `MOTOR_RUN_DUTY = 230`. LEDs are black
  during the ramp, then the LED animation and 1000 ms audio fade start together.
- A debounced 1.5 s encoder hold stops loads and KEY pulses. Sleep starts after
  release; no blocking release loop. GPIO4 LOW remains the powered-sleep wake.
- NFC uses one latest-presence queue instead of competing detection/removal
  flags. Removing a tag cancels pending motor startup. Missing/invalid files
  are retried only after tag removal/re-presentation, not in an endless loop.
- Setup failures leave loads off and require a restart after fixing the fault.
  Reset/wake causes are logged. PWM supports Arduino-ESP32 2.x/3.x APIs.
- Three plays per presentation, encoder volume, random patterns and fades are
  retained. Startup fade now also runs on replays. Very brief NFC transitions
  can be coalesced by the latest-state queue; this is a presence player, not an
  event recorder. SD/decoder library calls can still take time.

Arduino core and library versions still need an actual ESP32 build. Native
control-flow tests do not certify library compatibility, audio timing or wiring.

Local check: run `& 'C:\MyStuff\tests\Run-ControlTests.ps1'` in PowerShell.
It compiles the sketch against fake peripherals with MSVC and runs 115 assertions
for each PWM API branch (2.x and 3.x). Both passed on 2026-09-27. An actual Arduino
build was blocked: no board platforms installed and Arduino download DNS failed.
When building with Arduino IDE, use an empty `nfc_player.ino` sketch folder with
this `.cpp` copied beside it (or rename the file to the matching `.ino`, without
keeping a second compiled copy). Keep the host tests outside the Arduino sketch.

---

## GPIO map

| GPIO | Function      | Component        |
|------|---------------|------------------|
| 4    | SW            | Encoder button   |
| 5    | SS            | PN532            |
| 13   | CS            | SD reader        |
| 14   | SCK           | SD reader        |
| 15   | MOSI          | SD reader        |
| 16   | MISO          | SD reader        |
| 18   | SCK           | PN532            |
| 19   | MISO          | PN532            |
| 21   | DIN           | LED ring         |
| 22   | I2S DOUT      | MAX98357A        |
| 23   | MOSI          | PN532            |
| 25   | I2S LRC       | MAX98357A        |
| 26   | I2S BCLK      | MAX98357A        |
| 27   | MOSFET gate   | Motor            |
| 32   | CLK           | Encoder          |
| 33   | DT            | Encoder          |
| 2    | AMP_SD        | MAX98357A (mute) |
| 17   | KEY tap       | IP5310 KEY (via transistor) |

---

## Power variants

Choose Variant B for charging with the player physically switched off. Both
drawings assume NO DevKit USB connection. All peripheral supply branches must
be downstream of the load switch in Variant B, including any 3.3 V regulators.
Use the PCM manufacturer's actual B+/B-/P+/P- wiring, never guessed pin labels.

### Common battery protection

```text
Cell + ---- F1 close to cell ---- PCM B+
Cell - ------------------------ PCM B-
PCM P+ ------------------------ protected pack +
PCM P- ------------------------ protected pack - / system GND
```

Do not connect load/charger ground straight to cell negative when that bypasses
the PCM. For a cell with built-in protection, follow its specified pack terminals.
F1 is not automatically 3 A: select from measured charge/discharge current,
cell and wire ratings, fault current and the fuse time-current curve. A PTC
hold-current rating is not interchangeable with a cartridge fuse rating.

### Variant A: disconnect battery from charger/load

```text
Protected pack + ---- SW_A ---- IP5310 BAT+
Protected pack - -------------- IP5310 GND
Charger USB ------------------ IP5310 charging input
IP5310 OUT+ ---- optional D1 ---- LOAD_5V star
IP5310 GND --------------------- LOAD_GND star
```

Opening SW_A stops battery charging and battery supply to the IP5310. The PCM
may still draw its own quiescent current. A connected charger or DevKit USB can
still power parts of the system: this is NOT an all-source cutoff.

### Variant B: disconnect player, retain charging

```text
Protected pack + --------------- IP5310 BAT+
Protected pack - --------------- IP5310 GND
Charger USB ------------------- IP5310 charging input
IP5310 OUT+ ---- optional D1 ---- SW_B ---- LOAD_5V star
IP5310 GND ------------------------------ LOAD_GND star
```

SW_B removes supply to the entire player while charging remains possible.
IP5310/PCM still consume standby current. Closing SW_B may also require a KEY
tap to wake the boost. With DevKit USB connected, SW_B alone cannot guarantee off.

LOAD_5V feeds ESP32 VIN, amp VIN, LED ring and motor positive. PN532/SD supply
voltage depends on their actual breakout boards; all ESP32-facing signals must
remain 3.3 V compatible. Never feed 5 V into ESP32 3V3.

### D1 and programming USB

D1, if used, is a through-hole 1N5822 (3 A/40 V): anode to IP5310, band/cathode
to load. It blocks load-to-IP5310 current only and drops voltage under load;
its nominal current rating is not proof of sufficient thermal margin.

It does NOT block IP5310-to-PC current through an unisolated DevKit USB path.
For bench uploads disconnect external VIN and peripheral power/signal connections
that could back-power them, and power the DevKit from USB alone. Upload resets
the ESP32 and stops playback. Concurrent operation needs a verified board
schematic and reverse-current blocking on BOTH power paths, or a suitable power
mux. Adding a second diode without separating the existing USB/VIN connection
does not solve it. A VBUS-cut cable may not work with every USB-serial bridge.

---

## Motor driver (low-side, GPIO27)

```text
LOAD_5V ----+---- motor +
        +---- diode CATHODE (band)
        +---- 100 nF ----+
                  |
motor - -------------------+---- IRLZ44N DRAIN
diode ANODE -------------------- IRLZ44N DRAIN
IRLZ44N SOURCE ----------------- LOAD_GND
GPIO27 ---- 100 ohm ------------- IRLZ44N GATE
GATE ------ 10 kohm ------------- SOURCE

220 uF bulk: positive to LOAD_5V, negative to LOAD_GND (NOT to drain).
```

Use a fast Schottky for continuous 20 kHz PWM: 1N5819 (1 A/40 V) only if measured
motor current and transients fit its ratings, or a suitably rated 1N5822. Do not
use the 1N4007 here or decide suitability only by touching it for heat.

IRLZ44N is not guaranteed low-resistance at a 3.3 V gate. Keep it for measured
bench evaluation, not as a guaranteed design. Check drain/source voltage during
the ON interval and driver temperature. A through-hole TC4420CPA (non-inverting,
DIP-8, 5 V supply) is a possible gate-driver upgrade; verify its datasheet and
add local ceramic bypass. Retain a pulldown at its input and at the MOSFET gate.
Only buy it if measurements justify keeping/driving this MOSFET. A suitable
3.3 V-specified MOSFET/driver module is another option, but not required blindly.

PWM limits average applied voltage; it does not guarantee a stall-current cap.
The 600 ms ramp and starting duty must be checked with the actual belt/platter.

---

## IP5310 KEY driver (GPIO17) and one-button plan

Retain an already-working driver. For a new through-hole build the 2N3904
version below is straightforward once KEY voltage/current have been checked.
Check transistor pin order against the specific manufacturer's datasheet.

MOSFET alternative (verify 2N7000 pull-down performance at 3.3 V for this KEY):

```
   GPIO17 ─[1kΩ]─ Gate ──[2N7000]
                   │          Drain ── IP5310 KEY ──[momentary button]── GND
                [100kΩ]       Source ── GND
                   │
                  GND
```

BJT version:

```
   GPIO17 ─[1kΩ]─ Base ──[2N3904]
                   │          Collector ── IP5310 KEY ──[momentary button]── GND
                [10kΩ]        Emitter ── GND
                   │
                  GND
```

- GPIO17 HIGH → transistor pulls KEY to GND = simulated short tap (keep-alive).
- The button shown is a temporary normally-open bench button/jumper, not a
  requirement for another visible enclosure control.
- The gate/base pulldown is required so KEY isn't tapped during the reset
  high-Z window, regardless of brownout settings.
- GPIO17 avoids GPIO12's flash-voltage strap on classic ESP32. Confirm WROOM
  hardware; GPIO16/17 can be occupied by PSRAM on other module variants.
- Verify that 120 ms repeated taps really prevent shutdown and never toggle an
  unwanted mode. Do not infer KEY voltage or internal pull-up from its name.

### Keep one visible encoder, but test before choosing its circuit

The current firmware still expects an independent active-LOW GPIO4 switch.
Do not implement the previously suggested inverted KEY-sense circuit as-is.
KEY-generated keep-alive pulses would become false button presses, KEY may
have a weak pull-up, and a physical long hold invokes the IP5310's own behavior.

1. Measure KEY-to-GND voltage while output is on, asleep/off, and charging.
   Characterize short tap, 1.5-3 s hold and double tap using a temporary contact.
2. Identify whether your encoder switch has an onboard pull-up tied to 3V3.
   Never connect that network directly to KEY or assume it is safe unpowered.
3. If KEY long holds are harmless, a replacement encoder with genuinely isolated
   double-pole push contacts could avoid voltage-domain sharing: one pole for
   KEY/GND, the other for GPIO4/GND. Verify isolated contacts, availability and
   cold-start/hold behavior; common encoder modules do NOT provide this.
4. If keeping the present encoder is mandatory, design a battery-powered isolated
   wake/sense or pulse-shaping circuit after those measurements. Do not order a
   speculative diode-OR circuit. GPIO firmware alone cannot wake an unpowered MCU.
5. Decide the gesture after measuring KEY: long-press may need a different
   implementation if the IP5310 itself switches modes/off on that gesture.

This preserves the one-control goal without claiming an untested circuit works.

---

## Audio — MAX98357A (7-pin, independent SD & GAIN)

```
   ESP32                 MAX98357A
   GPIO26 (BCLK) ───────► BCLK
   GPIO25 (LRC)  ───────► LRC
   GPIO22 (DOUT) ───────► DIN
   GPIO2  (AMP_SD)──────► SD          (HIGH = on, LOW = mute)
                          GAIN ── leave floating = 9 dB
   5V ──────────────────► VIN         + 100µF + 100nF at VIN
   GND ─────────────────► GND
                          + ── Speaker +
                          − ── Speaker −
```

SD_MODE is shutdown/channel selection; GAIN is separate. Floating GAIN gives
the IC's nominal 9 dB setting only if the breakout has no overriding connection.
Inspect any SD pull-up to VIN before connecting GPIO2 (not 5 V tolerant and a
boot strap). Do not remove resistors blindly or assume LOW is maintained through
reset/sleep. Use a verified compatible pull-up/buffer arrangement; interface
polarity must match firmware. Never connect either speaker terminal to GND:
this is a bridge output. A fade/mute reduces transients but cannot promise no pop.

---

## WS2812 LED ring (GPIO21)

```text
GPIO21 -> SN74AHCT125N input 1A
SN74AHCT125N output 1Y -> 330 ohm -> LED DIN
SN74AHCT125N VCC -> LOAD_5V; GND and /1OE -> LOAD_GND
LED 5V -> LOAD_5V; LED GND -> LOAD_GND
470 uF + 100 nF across LED supply; 100 nF at the AHCT125 supply pins
```

Use the DIP-14 AHCT version for a through-hole 3.3 V-to-5 V buffer, not a plain
HC125 whose input threshold differs. Tie unused inputs to a defined level and
disable unused outputs. Keep data wiring short; never drive this unpowered
buffer/ring from a separately USB-powered ESP32. The 330 ohm resistor does not
raise a 3.3 V signal to a guaranteed 5 V-ring logic HIGH.

---

## PN532 (software SPI)

```
   GPIO18 ► SCK   GPIO19 ◄ MISO   GPIO23 ► MOSI   GPIO5 ► SS
   3V3 ► VCC      GND ► GND
```

## SD reader (HSPI)

```
   GPIO14 ► SCK   GPIO16 ◄ MISO   GPIO15 ► MOSI   GPIO13 ► CS
   5V or 3V3 ► VCC (per module)   GND ► GND
```

## Rotary encoder

```
   GPIO32 ◄ CLK   GPIO33 ◄ DT   GPIO4 ◄ SW
   + ► 3V3        GND ► GND      (internal pull-ups in firmware)
```

---

## Decoupling

| Node        | Caps                    |
|-------------|-------------------------|
| IP5310      | 1000µF + 100nF          |
| ESP32 VIN   | 470µF + 100nF           |
| MAX98357A   | 100µF + 100nF           |
| LED ring    | 470µF + 100nF           |
| Motor rail  | 220µF + 100nF (brushes) |

Yellow box-shaped 100 nF parts are often film, but appearance does not identify
them. Read markings/datasheet. Suitable film caps can suppress motor brush noise;
use short-leaded 100 nF X7R ceramics at logic/driver supply pins. A 16 V or higher
rating is suitable here. Electrolytics on 5 V should be rated at least 10 V and
installed with correct polarity. The motor's 100 nF is across its terminals;
the 220 uF is across the supply and ground.

470 uF at VIN is an optional measured trial, not a guaranteed fix. At 200 mA it
loses 1 V in about 2.35 ms. Added capacitance increases startup inrush. Compare
waveforms with/without it instead of stacking more capacitance for a seconds-long
IP5310 dropout. The DevKit already has local regulator decoupling; do not alter
its regulator capacitors without the board/regulator specifications.

---

## Critical notes

- **Star-ground:** run motor and LED grounds back to the IP5310 GND directly,
  not through the ESP32, so their current pulses don't corrupt audio/PN532 grounds.
- Route star returns to protected system ground, not bare cell negative.
- Do not route motor/amp/LED current through solderless breadboard contacts for
  the final load test. Use short adequately rated wires, connectors and joints.
- USB isolation, fuse/PCM selection and KEY sharing remain measurement gates,
  not solved by the diagrams alone. Both switches above need DC current ratings.
- **KEY gate/base pulldown** is required during reset; brownout stays enabled.
- ESP32 is flashed via its **own USB**; the IP5310 charges via **its own USB** —
  two separate ports.
- Gate resistor is **100 Ω** (not 10 Ω); gate pulldown **10 kΩ**.

---

## Turntable drive (belt / O-ring, 33 1/3 RPM)

Motor: **N20, 50 RPM @ 6 V** (2-wire). The estimate `50 * 5.1/6 = 42.5 RPM`
is only a starting guess: measure at the actual motor supply, after any diode,
with belt load. There is no justified fixed 10-20% load correction. Open-loop
PWM is sufficient for your approximate decorative 33 1/3 RPM target, not precision.

### Drivetrain
```
  Motor shaft ──[~20 mm pulley]──O-ring──[~20 mm sub-pulley]── M8 spindle
                                                                 │
                                                          608ZZ bearing
                                                                 │
                                                     balanced platter on top
```

- Start with near-equal pulleys of about **20 mm effective belt diameter**,
  leaving material around an 8 mm spindle bore. Check groove depth, hub wall
  thickness and fastening before making them. Speed ratio is motor RPM times
  motor belt diameter divided by driven belt diameter, neglecting slip.
- Use a balanced ~120 mm decorative platter, not arbitrarily heavy: extra
  inertia can smooth rotation but increases startup load. Use a smooth 8 mm
  bearing seat rather than threads as a precision shaft; do not clamp across
  both bearing races. Check spindle wobble and axial retention.
- Keep both pulley grooves at the **same height** so the O-ring runs flat; a slight
  groove/crown keeps it tracking.
- Start at **50 mm center distance** with slotted motor mounting for adjustment.
  Center distance sets belt fit, not speed ratio. Fit this under the platter in
  the 200 x 150 mm box after allowing for wall thickness and component heights.
- Approximate open-belt centerline length:
  `L = 2C + (pi/2)(D+d) + (D-d)^2/(4C)`, using effective belt-center diameters.
  For C=50 mm and D=d=20 mm, L is about 162.8 mm. A 2 mm-section O-ring with
  48 mm ID has free centerline length `pi*(48+2) = 157.1 mm`, about 3.7% nominal
  stretch to fit. This is a trial size, not guaranteed: groove seating and
  cross-section deformation matter. Use the least tension that avoids slipping.

### Firmware speed control
- `MOTOR_RUN_DUTY` sets steady duty; current configuration requires
  `MOTOR_DUTY_MIN <= MOTOR_RUN_DUTY <= 255`. Default **230**, not a calibration.
- Continuous 20 kHz PWM requires the fast flyback diode described above.

### Tuning to 33 1/3 RPM (do this when the box is built)
1. Put a mark on the platter. **33 1/3 RPM = one full turn every 1.8 s.**
   (45 RPM = 1.333 s/turn, for reference.)
2. Measure: phone strobe/RPM app, or count 10 turns and divide the time by 10
   (should be ~18 s for 33 1/3).
3. Adjust **`MOTOR_RUN_DUTY`** and re-flash:
   - Platter too **slow** → raise the value (toward 255).
   - Platter too **fast** → lower the value.
   - Change by about 5 counts initially; speed is not strictly linear in duty.
     Stop if the motor stalls or cannot start with the belt installed.
4. Re-check after adding the real platter/belt — load changes the working point.

### If tuning hits a limit
- Can't reach 33 1/3 even at `MOTOR_RUN_DUTY = 255` → the pulley reduction is a
  touch too much: make the **sub-pulley slightly smaller** (or the motor pulley
  slightly larger).
- Platter too fast even at low duty (runs rough/cogging at the bottom of the
  range) → add a little more reduction: **larger sub-pulley**, so the motor runs
  at a healthier higher duty for the same platter speed.
- Speed drifts/wobbles: check supply, belt tension, bearing alignment and groove
  concentricity before adding mass. Time 30 turns (~54 s) to reduce timing error.

## Parts: buy now versus decide after measurement

| Part | Action |
|---|---|
| Through-hole Schottky flyback | Buy 1N5819 if motor-current ratings fit; 1N5822 is a higher-current candidate. |
| 1N5822 for optional rail D1 | Retain if fitted; it is not complete USB isolation. Check voltage drop and heating. |
| SN74AHCT125N, DIP-14 | Recommended for the 5 V LED data input; add 100 nF ceramic bypass. |
| 100 nF X7R ceramics, >=16 V | Buy for local logic/driver bypass if your existing parts are unidentified film. |
| 2N3904, 1 kohm, 10 kohm | Only if the GPIO17 KEY pulldown is not already built and working. |
| Protected cell or documented 1S PCM | Required for an unprotected cell; choose to cell current rating, actual load and charger current, not a blanket 4-6 A rule. |
| DC-rated fuse holder and wiring | Fit close to cell; final fuse rating awaits measured current and wire/cell limits. |
| 470 uF >=10 V electrolytic | Optional VIN comparison test, not a cure for converter shutdown. |
| TC4420CPA DIP-8 driver | Conditional upgrade for IRLZ44N, only after gate/ON-voltage checks. |
| Extra external button, P-MOSFET, replacement boost | Do not buy yet. One-encoder circuit and power gating await measurements. |

The BSMPCM 3 A board might fit, but its model name alone is insufficient. Obtain
continuous-current, trip/delay, charge limits and cell-compatibility data. Battery
current differs from 5 V load current: `Ibat ~= Vout*Iout/(Vbat*efficiency)`.
For 5 V/2 A at 3 V and 85%, this is ~3.9 A. Do not simply add charging current
to discharge current: the battery carries net current according to the power path.

## Walk-through: stop at the first failed step

1. **Document the boards.** Photograph both sides of IP5310/K648, DevKit expansion,
  amp and encoder; record cell model/rating and Arduino core/library versions.
  Inspect for damage after the previous overvoltage incident. No combined USB/VIN.
2. **Protect and test the supply alone.** Verify PCM/fuse wiring and capacitor
  polarity. Use a current-limited bench source where available. Measure battery,
  OUT+ and ground wiring under a known dummy load, not the whole player. Example:
  100 ohm/1 W at 5 V draws 50 mA and dissipates 0.25 W; resistor gets warm.
  This load is a diagnostic, NOT a permanent keep-alive workaround.
3. **Characterize KEY and charger transitions.** Record the KEY measurements and
  gestures above. Check OUT+ during charger connect/disconnect at no load and
  known load. No firmware runs in this test, so failures here cannot be repaired
  by ESP32 startup sequencing. Check the module's documented pass-through support.
4. **Test ESP32 alone.** Upload using USB with external wiring disconnected as
  described above, then test battery-powered with brownout enabled. Observe
  reset reason, 5 V VIN and 3.3 V during startup/transitions. A scope is preferred;
  a meter can miss short dips and overshoot. Use a verified isolated monitoring
  arrangement before reconnecting a PC; do not reintroduce USB back-feed to log.
5. **Add SD/PN532, then LEDs and muted amp.** Verify module supply/logic ratings.
  Run for longer than several 20 s keep-alive intervals. Confirm no false KEY
  modes, rail collapse, overheating or initialization failures. Fix each before
  adding the next load. LEDs at black still consume standby current.
6. **Add motor last.** Fit fast flyback and correct capacitor placement first.
  Check start without belt, then with belt/platter. Measure KEY pulse length,
  battery current, motor ON-state drain voltage and 3.3 V during starts. Do not
  use prolonged stalls to measure current; use datasheet or a controlled short
  current-limited test. Software PWM is not an overcurrent limiter.
7. **Exercise firmware.** Remove/swap tag during ramp; present a missing file;
  complete three repeats; adjust volume during fade; hold/release encoder during
  startup and playback. Verify motor/amp stop on sleep request and KEY releases.
  Check encoder wake while still powered, and KEY cold-start after actual bank
  shutdown. Measure idle current rather than assuming sleep equals power-off.
8. **Choose one-button hardware and USB strategy.** Only now commit the tested
  KEY/encoder interface and charging/programming power path. Until then, the
  temporary KEY contact stays a bench aid, not a second enclosure control.
9. **Calibrate and package.** Tune RPM with final belt tension, mount PN532 away
  from metal spindle/motor/battery and test through the lid. Keep switching wires
  away from NFC/audio wiring, provide cell insulation and access for servicing,
  then repeat charger transition tests with playback. Accept controlled reset
  recovery if the supply cannot provide uninterrupted handover; do not disable
  brownout. Update/review KiCad before ordering a PCB.

