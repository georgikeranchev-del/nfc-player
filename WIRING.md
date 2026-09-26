# NFC MP3 Player — Wiring Reference

Companion wiring for `nfc_player.cpp` (ESP32 DevKit v1).

Two power variants are provided:

- **Variant A — Hard battery cutoff:** the switch fully disconnects the battery.
  True zero drain, but **cannot charge while off**. Best for storage/transport.
- **Variant B — Output-only cutoff:** the switch only removes 5 V from the load
  (ESP32 + peripherals). The charger→battery path stays intact, so the battery
  **can still charge while the player is off**. The IP5310 keeps a tiny quiescent
  draw from the battery.

Everyday "off" in both variants is the IP5310 auto-shutdown (low-load) + KEY
button / encoder long-press — and charging works in that state regardless of variant.

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
| 12   | KEY tap       | IP5310 KEY (via transistor) |

---

## Power — Variant A: Hard battery cutoff (no charge when off)

```
   USB-C (charger) ──► IP5310 VIN (charge input)
                                │
   Li-ion + ─[3A FUSE]─[HARD SW]─► IP5310 BAT   <-- fuse + switch on battery line
   Li-ion − ───────────────► IP5310 GND ──── COMMON GND (star point)
                                │
                          IP5310 VOUT (5V)
                                │
                          ─[1N5822]─►──┬──► ESP32 VIN (5V)
                          (blocks USB   ├──► MAX98357A VIN
                           back-feed)   ├──► WS2812 ring 5V
                                        └──► Motor rail (5V)
                          IP5310 KEY ──┬──[KEY button]── GND
                                       └── transistor drain (keep-alive)

   HARD SW open  => battery disconnected => NO charging, NO output (true off).
```

## Power — Variant B: Output-only cutoff (can charge when off)

```
   USB-C (charger) ──► IP5310 VIN (charge input)
                                │
   Li-ion + ─[3A FUSE]─────► IP5310 BAT        <-- fuse; battery always connected
   Li-ion − ───────────────► IP5310 GND ──── COMMON GND (star point)
                                │
                          IP5310 VOUT (5V)
                                │
                          ─[1N5822]─►─[HARD SW]──┬──► ESP32 VIN (5V)
                          (back-feed   (switch on  ├──► MAX98357A VIN
                           block)       the load    ├──► WS2812 ring 5V
                                        rail)        └──► Motor rail (5V)
                          IP5310 KEY ──┬──[KEY button]── GND
                                       └── transistor drain (keep-alive)

   HARD SW open  => load off, but VIN->BAT charging still works.
   IP5310 still draws its small quiescent current from the battery.
```

> In both variants the **1N5822 Schottky** (3 A, through-hole; band/cathode toward
> the ESP32) sits on VOUT so PC-USB (programming) and the IP5310 don't back-feed
> each other. In Variant B the hard switch is placed *after* the diode, on the
> load rail.
>
> The **3 A fuse** sits in the battery + line, as close to the cell as possible,
> so it protects both the discharge and charge paths against a short.

---

## Motor driver (low-side, GPIO27)

```
                5V rail
                  │
              +──[MOTOR]──+     100nF across motor terminals
              │           │
   (cathode)│▼│ 1N4007    │     flyback: cathode->5V, anode->drain
              └───────────┤
                          │ Drain
   GPIO27 ─[100Ω]─Gate──[IRLZ44N]
                    │            Source ── GND
                 [10kΩ]
                    │
                   GND
        (+ 220µF from 5V->GND on the motor rail)
```

> Turntable speed is set by `MOTOR_RUN_DUTY` in firmware (continuous PWM). If the
> 1N4007 runs warm under continuous 20 kHz switching, swap it for a **1N5819**
> Schottky. For an N20's small current it's usually fine either way.

---

## IP5310 KEY driver + power-on button (GPIO12)

MOSFET version (recommended):

```
   GPIO12 ─[1kΩ]─ Gate ──[2N7000]
                   │          Drain ── IP5310 KEY ──[momentary button]── GND
                [100kΩ]       Source ── GND
                   │
                  GND
```

BJT alternative:

```
   GPIO12 ─[1kΩ]─ Base ──[2N3904]
                   │          Collector ── IP5310 KEY ──[momentary button]── GND
                [10kΩ]        Emitter ── GND
                   │
                  GND
```

- GPIO12 HIGH → transistor pulls KEY to GND = simulated short tap (keep-alive).
- The **momentary button** (normally-open, non-latching) across KEY↔GND is the
  manual power-on.
- The gate/base pulldown is required so KEY isn't tapped during the reset
  high-Z window (brown-out detector is disabled in firmware).

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

---

## WS2812 LED ring (GPIO21)

```
   GPIO21 ─[330Ω]─► DIN
   5V ────────────► 5V     + 470µF across 5V/GND at the ring
   GND ───────────► GND
```

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
| ESP32       | 100nF                   |
| MAX98357A   | 100µF + 100nF           |
| LED ring    | 470µF + 100nF           |
| Motor rail  | 220µF + 100nF (brushes) |

100nF can be ceramic (MLCC) or polyester film — both fine here; rating ≥ 16 V.

---

## Critical notes

- **Star-ground:** run motor and LED grounds back to the IP5310 GND directly,
  not through the ESP32, so their current pulses don't corrupt audio/PN532 grounds.
- **1N5822 Schottky** (3 A, through-hole) on VOUT→VIN prevents PC-USB ↔ IP5310
  back-feed while programming; band/cathode toward the ESP32.
- **3 A fuse** in the battery + line (at the cell) guards against shorts. A
  resettable PTC (~2–3 A hold) works too. Prefer a **protected Li-ion cell / BMS**
  as the primary safety layer; the fuse is a backup.
- **KEY gate/base pulldown** is essential (brown-out detector disabled in firmware).
- ESP32 is flashed via its **own USB**; the IP5310 charges via **its own USB** —
  two separate ports.
- Gate resistor is **100 Ω** (not 10 Ω); gate pulldown **10 kΩ**.

---

## Turntable drive (belt / O-ring, 33 1/3 RPM)

Motor: **N20, 50 RPM @ 6 V** (2-wire, no encoder). At 5.1 V it free-spins at
about `50 × 5.1/6 ≈ 42.5 RPM`, dropping ~10–20% under belt load. Speed is set
**open-loop** by the `MOTOR_RUN_DUTY` PWM value in firmware — no feedback needed.

### Drivetrain
```
   Motor shaft ──[8 mm pulley]──O-ring──[~9-10 mm sub-pulley]── M8 bolt
                                                                 │
                                                          608ZZ bearing
                                                                 │
                                                     heavy platter on top (flywheel)
```

- Motor pulley Ø **8 mm**, driven sub-pulley Ø **~9–10 mm** (barely any reduction —
  the motor is already close to platter speed).
- The **belt rides on the sub-pulley**; a heavy decorative platter (~120 mm) on the
  same M8 spindle adds flywheel inertia to smooth out wow/flutter (matters more
  than any code, since there's no speed feedback).
- Keep both pulley grooves at the **same height** so the O-ring runs flat; a slight
  groove/crown keeps it tracking.
- O-ring: stretch ~5–7 % for grip. Belt path length
  `L = 2C + (π/2)(D+d) + (D−d)²/(4C)`, where C = center distance, D/d = pulley Ø.
  Example C ≈ 50 mm, D = 10, d = 8 → L ≈ 128 mm → O-ring ~38 mm ID × 2 mm.

### Firmware speed control
- `MOTOR_RUN_DUTY` (0–255) sets the steady platter speed; the soft-start ramps up
  to it and holds. Default **230**. Higher = faster.
- This is **continuous 20 kHz PWM**. If the 1N4007 flyback runs warm, swap to a
  **1N5819** Schottky (usually fine as-is for an N20's small current).

### Tuning to 33 1/3 RPM (do this when the box is built)
1. Put a mark on the platter. **33 1/3 RPM = one full turn every 1.8 s.**
   (45 RPM = 1.333 s/turn, for reference.)
2. Measure: phone strobe/RPM app, or count 10 turns and divide the time by 10
   (should be ~18 s for 33 1/3).
3. Adjust **`MOTOR_RUN_DUTY`** and re-flash:
   - Platter too **slow** → raise the value (toward 255).
   - Platter too **fast** → lower the value.
   - Rough feel: each ±10 in duty ≈ a few % speed change (not perfectly linear;
     there's a stall deadband at the low end).
4. Re-check after adding the real platter/belt — load changes the working point.

### If tuning hits a limit
- Can't reach 33 1/3 even at `MOTOR_RUN_DUTY = 255` → the pulley reduction is a
  touch too much: make the **sub-pulley slightly smaller** (or the motor pulley
  slightly larger).
- Platter too fast even at low duty (runs rough/cogging at the bottom of the
  range) → add a little more reduction: **larger sub-pulley**, so the motor runs
  at a healthier higher duty for the same platter speed.
- Speed drifts/wobbles → heavier platter, check belt tension (not too tight →
  motor bogs; not too loose → slips), and make sure the O-ring isn't riding
  on a rough/eccentric groove.

