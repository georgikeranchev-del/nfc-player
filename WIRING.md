# Battery-Only Perfboard Wiring

This is the new baseline, superseding contradictory advice in the old chat and
parent wiring file. Keep the existing classic ESP32 development board, IP5310/K648,
PN532, SD module, MAX98357A, ring, N20 motor and encoder. Reuse their existing
capacitors if identified and undamaged. No TP4056/alternative boost is required.

Reported silicon is **ESP32-D0WD-V3, revision v3.1, with a 40 MHz crystal**.
This does not confirm the development-board/module model or PSRAM. WROOM without
PSRAM is the working assumption from the reported earlier check, not a verified
module identity. Use [PINOUT.md](PINOUT.md) for the complete GPIO table, GPIO16/17
restrictions and compact connection schematic. Full reported chip details are in
[README.md](README.md#detected-esp32).

## What to buy

- **1 x SN74AHCT125N, DIP-14, plus socket:** LED level shift and amp SD isolation.
- **1 x TC4420CPA, DIP-8, plus socket:** non-inverting 5 V motor gate driver for
  the IRLZ44N you own; check the exact suffix is PDIP, not SMD/inverting TC4429.
- **1 x 1N5822 axial Schottky:** motor flyback baseline. A 1N5819 is an alternative
  only if its 1 A/transient ratings fit your measured motor. No 1N4007 for PWM.
- **100 nF X7R ceramics, >=16 V:** buy a small pack for each logic/driver supply;
  one 1 uF ceramic at the gate driver. Yellow box film caps can remain for motor
  brush suppression once their value/rating is confirmed.
- **Resistors:** keep motor 100 ohm and gate/source 10k, LED data 330 ohm. Add
  10k pulldowns at gate-driver input, amp-buffer input, LED-buffer input; KEY
  NPN needs 1k base series and 10k base/emitter. Buy extras as normal stock.
- **2N3904, TO-92** if no working KEY pull-down circuit exists. Verify its pinout.
- **Cell protection and fuse hardware:** documented protected 1S cell or compatible
  1S PCM, DC-rated fuse holder, rated battery holder/leads/connectors and heatshrink.
  Final fuse/PCM current is a measurement decision, not a generic 3 A shopping rule.
- **Perfboard, sockets/header strips, rated connectors, insulated wire, standoffs**
  and a DC-rated SPST load switch if you want physical off. Use mounting support
  for the encoder; do not support the box/knob by solder joints alone.
- **Optional fallback:** one normally-open momentary KEY button. Leave a service
  connector first so the one-external-button decision can wait. No new encoder
  is needed unless the existing one is mechanically unreliable.

Exact inventory and quantities are in [BOM.csv](BOM.csv). A gate driver and logic
buffer do not solve boost overload, poor wiring, inappropriate battery selection,
or a mechanically stalled platter.

## Battery, protection and main switch

```text
Cell + -- F1 (close to cell) -- PCM B+
Cell - ---------------------- PCM B-
PCM P+ ---------------------- IP5310 BAT+
PCM P- ---------------------- IP5310 battery return / system GND

IP5310 OUT+ -- SW_LOAD ------- LOAD_5V star
IP5310 OUT- ----------------- LOAD_GND star

LOAD_5V -> ESP32 VIN/5V, amp VIN, ring 5V, motor +, AHCT125 VCC, TC4420 VDD
LOAD_GND -> all module returns, MOSFET source, driver/buffer GND, KEY emitter
```

Use the PCM manufacturer's real terminals, not inferred positions. A protected
cell already contains its own circuit; select one documented protection solution.
Never bypass low-side PCM protection by connecting load ground to bare cell -.
Size switch, wiring, connector and fuse for BATTERY current as well as load peaks.
Fuse coordination must allow expected transients but protect the weakest wire
and cell limits. Do not intentionally short a lithium cell to test protection.

The load switch leaves charging possible later (but no charger is connected for
this milestone). For storage, a battery disconnect can be added instead/before
the charger; with it open, that cell cannot charge. Either switch can be bypassed
by an unintended USB power path, so keep USB disconnected in battery tests.

For battery-only development omit the former series rail diode. Route IP5310
output straight through the load switch to the star. Do not put motor/amp current
through the ESP32 headers, skinny daisy-chain jumpers or long perfboard solder
tracks. Use short power wires and pair each signal with a nearby return path.

### 18650 selection

**A single suitable 18650 is a reasonable choice**, subject to the measured load
and the actual K648 charging configuration. "18650" describes the cell size, not
its chemistry, current capability or protection. Keep this a 1S design: do not
connect two cells in series or improvise a parallel pack to cure startup faults.

| Parameter | What to look for |
|---|---|
| Chemistry/voltage | Conventional rechargeable Li-ion, nominal 3.6/3.7 V, charged to 4.20 V. Verify K648 is configured for that cell. LiFePO4 or a different charge-voltage chemistry is not a drop-in substitute. |
| Capacity | Genuine 2500-3500 mAh is a practical shopping range; around 3000 mAh is a useful starting point. Capacity mainly determines runtime, not whether motor startup succeeds. |
| Discharge rating | Prefer a documented continuous discharge rating of at least 10 A as a shopping target with margin; ignore unqualified "pulse/max" marketing. Final suitability depends on measured current, sag and temperature. This is not a 10 A fuse recommendation or an IP5310 output rating. |
| Charge rating | Check the cell's recommended and maximum charge current separately from its discharge rating. The actual K648 cell-charging current must comply; USB input current is not the same measurement. If incompatible, reconfigure only by the module's documented method or select a compatible cell before charging. |
| Protection | Use a documented protected cell/pack OR a suitable external 1S PCM. Its charge/discharge limits, trip thresholds and wiring must suit the cell and measured startup current. A high-current bare cell behind a low-current protection board is still limited by that board. |
| Construction | Buy from a reputable battery supplier with a manufacturer datasheet and traceable model. Reject damaged wraps/positive-terminal insulators, dents and unknown salvaged cells. Avoid implausible capacity claims. |
| Fit/connections | Protected/button-top cells may be longer than a bare 65 mm cell. Check the exact holder dimensions and contact-current rating. Use a rated holder or professionally assembled pack with welded tabs/leads; do not solder directly to the cell. |
| Temperature/voltage limits | Follow the cell datasheet's operating and charging temperatures and minimum voltage. Choose an operating cutoff with sag margin above the minimum/protection trip. Supervise development charging; do not assume the module has cell-temperature sensing. |

A higher-current cell does not force current into the load, but it can deliver
more current into a fault: protection, a correctly coordinated fuse and insulated
connections remain necessary. It cannot increase the IP5310 module's own output
capability. A healthy existing cell that meets the measured requirements need
not be replaced just to obtain a larger mAh number.

Size the battery path from battery current, not the 5 V current. Approximately:

$$
I_{cell} \approx \frac{V_{load} I_{load}}{V_{cell} \eta}
$$

For example, a 5.1 V load drawing 2 A needs about **3.75 A from a 3.2 V cell** at
an assumed 85% boost efficiency, before allowing for transients. These are sizing
examples, not measured load/efficiency or a recommended discharge endpoint. Check
startup at the lowest intended battery level: the same output power needs more
battery current as voltage falls.

For a first runtime estimate:

$$
t_{hours} \approx \frac{V_{nominal} C_{Ah} \eta}{P_{load,average}}
$$

A 3.6 V, 3000 mAh cell contains nominally 10.8 Wh. At an assumed 85% efficiency,
that gives roughly **1.8 hours at 5.1 V / 1 A average load**, or **0.9 hours at
2 A**. Usable capacity, cutoff, motor/audio peaks, temperature and cell condition
can reduce this. Measure average power with representative music and lighting;
do not treat the full labelled capacity as mAh available at 5 V.

## Motor: driven gate, low-side switching

```text
GPIO27 -- TC4420 INPUT
             |
           10k
             |
            GND

TC4420 VDD -> LOAD_5V; GND pins -> LOAD_GND
100 nF + 1 uF between TC4420 VDD and GND, close to pins
TC4420 OUTPUT -- 100 ohm -- IRLZ44N GATE
IRLZ44N GATE -- 10k ------- IRLZ44N SOURCE -> LOAD_GND
IRLZ44N DRAIN ------------ MOTOR -
LOAD_5V ----------------- MOTOR +

Flyback diode: ANODE -> drain/motor -; CATHODE (band) -> motor +/LOAD_5V
100 nF across motor terminals; 220 uF from LOAD_5V to LOAD_GND, NOT drain
```

Wire all required duplicate driver GND/output pins per its datasheet, leave NC
pins as specified. Do not guess package pin numbers from another seller's board.
TC4420 accepts TTL-style input at a 5 V supply; its minimum supply is 4.5 V.
Gate/input pulldowns are mandatory even with brownout enabled. Check on-state
drain voltage and temperature under the actual belt load. Protect against exposed
MOSFET tab contact: the IRLZ44N tab is electrically connected to drain.

## DIP AHCT125: LED and amplifier

Pin numbers below are for SN74AHCT125N DIP-14, top-view datasheet numbering:

| Connection | Pin(s) |
|---|---|
| VCC -> LOAD_5V, GND -> LOAD_GND | 14, 7 |
| Channel 1 /OE -> GND | 1 |
| GPIO21 -> 1A, 10k from 1A to GND | 2 |
| 1Y -> 330 ohm -> LED DIN | 3 |
| Channel 2 /OE -> GND | 4 |
| GPIO2 -> 2A, 10k from 2A to GND | 5 |
| 2Y -> amplifier SD/SD_MODE | 6 |
| Unused /3OE and /4OE -> VCC | 10, 13 |
| Unused 3A and 4A -> GND; outputs unconnected | 9, 12; outputs 8, 11 |

Add 100 nF at pins 14/7. AHCT accepts 3.3 V HIGH when supplied at 5 V; plain HC
does not offer the same guarantee. Keep LED data short. Tie all grounds together.
Do not power the ESP32 alone while its GPIOs drive an unpowered AHCT/module.

**Rewire GPIO2:** it now goes to AHCT input, NOT directly to amp SD. This prevents
an amp-board 5 V pull-up from reaching a non-5-V-tolerant ESP32 boot pin. Verify
the SD pin is not hard-strapped to VIN; a hard strap must be resolved from the
board schematic before driving it LOW. A normal weak pull-up can be overridden
by the buffer. Do not blindly remove a GAIN or SD resistor.

MAX98357A BCLK=GPIO26, LRC=GPIO25, DIN=GPIO22. GAIN is independent of SD; leaving
it open gives nominal 9 dB only if the breakout does not override it. SD HIGH
selects a channel as well as enabling output. Check mono/stereo content. Speaker
goes between amplifier + and - outputs; NEITHER speaker lead connects to ground.

## KEY and encoder

```text
GPIO17 -- 1k -- 2N3904 BASE
BASE ----- 10k -- EMITTER -> system GND
COLLECTOR ---------------- IP5310 KEY
KEY ------ [temporary NO contact / optional wake button] ------ GND

Encoder SW -> GPIO4; other switch terminal -> GND
Encoder A/CLK -> GPIO32; B/DT -> GPIO33; common -> GND
Encoder breakout VCC -> 3V3 only, if the module needs it
```

The transistor emulates a tap while ESP32 is running. It cannot initiate its own
cold boot. Never connect KEY directly to GPIO4/17. Retain your already-tested
2N7000 circuit instead if fitted; do not build both driver alternatives.

The KEY voltage and press-characterization procedure is in
[MANUAL_TESTS.md](MANUAL_TESTS.md#key-characterization).

Keep the encoder and KEY on separate headers during bring-up. See the one-control
decision in [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md); no unmeasured isolation
circuit is described as solder-ready.

## SD, PN532 and decoupling

Use [PINOUT.md](PINOUT.md). PN532 must be set to SPI. Confirm each breakout's allowed
power voltage and 3.3 V logic levels; a module labelled 5V may have an onboard
regulator, while a bare SD/PN532 device cannot simply take 5 V. Supply these
modules only according to their own schematic, not from the general 5 V list.

| Location | Existing/additional capacitance |
|---|---|
| IP5310 output | Existing 1000 uF + 100 nF; verify startup with this load |
| ESP32 VIN | Existing board capacitors + local 100 nF; extra 470 uF only as a measured trial |
| MAX98357A VIN | 100 uF + 100 nF |
| LED ring | 470 uF + 100 nF |
| Motor rail | 220 uF to GND, 100 nF across motor brushes |
| AHCT125 | 100 nF ceramic |
| TC4420 | 100 nF + 1 uF ceramic |

Electrolytics on 5 V: >=10 V rating, correct polarity. Yellow plastic box caps
are often film; read markings rather than assume. Short-leaded ceramics belong
at logic/driver supply pins. Large bulk caps cannot cover seconds-long shutdown
and can worsen inrush. Do not keep increasing them without waveforms.

## Perfboard assembly and acceptance

Follow the staged assembly/acceptance checklist in
[MANUAL_TESTS.md](MANUAL_TESTS.md#perfboard-assembly-and-acceptance). That guide
also owns the battery-only test matrix, motor-ramp comparison, measurement log
fields and later charging/dual-USB gates. Those procedures are not recorded
hardware passes or authorization to connect unverified supplies.

## Platter tuning

Keep the existing N20. Start with near-equal pulleys around 20 mm effective belt
diameter so an 8 mm spindle bore has adequate wall material. About 50 mm shaft
spacing with a slotted motor mount is a reasonable layout trial, not a speed
setting. Keep the platter balanced, not arbitrarily heavy. Use a smooth 8 mm
bearing seat and don't clamp the 608ZZ's two races together.

Measure and adjust speed using
[MANUAL_TESTS.md](MANUAL_TESTS.md#platter-speed-check).