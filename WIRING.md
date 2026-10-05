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

## Perfboard connectors, wire and sockets

The selected ICs remain **TC4420CPA** and **SN74AHCT125N**. The previously discussed
TC4427/74HCT126 alternatives are not adopted; do not mix their pinouts or enable
wiring with this build.

Soldering module power leads directly to their labelled supply/GND pads is fine.
Terminate the other ends at rated, labelled terminal blocks on the perfboard so
modules remain removable. Secure the cable independently of the solder pad and
insulate exposed joints. Do not hang stiff wire or a heavy capacitor from a small
breakout pad. This advice is for module pads, not soldering directly to a cell.

| Proposed copper cross-section | Use and conditions |
|---|---|
| 1.5 mm2 | Ample starting point for short main power/return buses at the expected few-amp scale. It may be too stiff or large for perfboard holes; use supported distribution terminals or an insulated bus, not enlarged pads or a chain of solder blobs. |
| 0.5 mm2 | Reasonable for short individual motor, amplifier and LED power/return branches. Verify voltage drop and temperature at measured current, plus the terminal/connector rating and fuse coordination. |
| 0.33 mm2 | Electrically ample for GPIO signals; often larger than necessary. For SPI/I2S/LED data, short routing and nearby return paths matter more than copper area. |

These are conditional wiring choices, not certified current ratings. Battery-side
current can exceed 5 V load current. Include both outgoing and return conductors
when calculating voltage drop; connector contacts and thin module traces may
limit the circuit before the wire does. Keep motor/amp/ring returns separate back
to the distribution point, not through ESP32 headers or a signal-ground jumper.

Use terminal blocks specified for the conductor size and measured current. For
stranded wire under screw clamps, use suitable ferrules where the terminal maker
permits them; do not tin the clamped ends because solder can creep and loosen.
Use proper crimps/torque and provide strain relief. Soldered ends at PCB pads are
a different case and should be wetted normally. Label 5 V, 3V3, GND and signals;
different module supply voltages are not interchangeable.

Keep SPI and I2S wiring as short as practical, initially around 10 cm or less
where layout permits; this is a layout goal, not a guaranteed maximum. Route
clock/data with a nearby ground return and away from motor/gate/speaker wiring.
Route each motor or supply outgoing/return pair together. Twist the two speaker
leads together; neither speaker lead is a ground return.

Good-quality DIP-8 and DIP-14 IC sockets on perfboard are appropriate. Align the
socket notch and IC pin 1, inspect solder joints before inserting ICs, and insert
or remove only with all power disconnected. Keep the gate resistor, pulldowns
and driver bypass close to their relevant pins, not at the far end of a cable.

Headers/sockets also make the DevKit removable. If the DevKit is permanently
soldered, provide disconnectable connections for its complete player harness,
including supply and GPIO paths. The present USB programming procedure requires
that isolation; disconnecting only VIN while leaving GPIOs connected is not enough.

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

### Fuse and PCM during startup

**A 3 A marking is not an instantaneous cutoff at 3.001 A.** The BOM still leaves
fuse and protection selection open; no particular 3 A part is approved here.

- An ordinary cartridge/blade fuse is a one-time protective component. A short
  pulse above its rating may survive, while a larger/longer overload can melt it.
  Use the exact fuse's time-current curve, pulse endurance, ambient derating and
  DC breaking rating. Fast-acting and time-delay 3 A fuses need not behave alike.
  If it blows, correct the cause before replacing it with the approved type; do
  not bridge it or simply install a higher rating.
- A resettable PTC is different: it heats into a high-resistance state and may
  recover after the fault is removed and it cools. Hold/trip current, resistance
  and delay differ from a fuse. It is not an automatic drop-in substitute here.
- A PCM normally turns off its protection MOSFETs on a detected fault rather than
  consuming itself. Its advertised continuous rating, overcurrent trip threshold,
  detection delay and permitted surge rating are separate specifications. A
  "3 A" board may trip above 3 A, but that does not authorize sustained overload.
  Recovery may require load removal or a manufacturer-specified charger event;
  never short pads to reset it. A poorly rated, overheated or miswired board can
  still be permanently damaged, including failure to protect.

Coordinate the fuse, PCM, cell and weakest wire/contact with the measured battery
current and surge duration. The motor ramp does not control all cold-start inrush:
charging the capacitors and booting the boost happen before the ramp. If normal
running already exceeds a PCM's continuous rating, a longer ramp cannot fix it.
Repeated trips, hot protection parts or blown fuses are faults to investigate,
not expected startup behavior. Keep the load off until the cause is understood.

### Connecting the 18650 holder and PCM

The diagram above applies to a **documented 1S, 4.20 V Li-ion, common-port PCM**:
the same protected P+/P- port carries discharge and later charge current. It is
not a pin-position guide for an unidentified board. The PCM protects the cell;
the IP5310 performs charging/boost conversion. A 2S/3S board is not suitable.

For a bare cell in its holder, the electrical connections are:

```text
Holder + --> F1 close to holder + --> PCM B+
Holder - --------------------------> PCM B-
PCM P+ ----------------------------> IP5310 BAT+
PCM P- ----------------------------> IP5310 BAT-/protected system return
```

All player grounds stay on the protected side. Do not connect holder - directly
to ESP32, amplifier, motor-driver or USB ground around the PCM. Some common-port
boards share B+ and P+ copper; others label pads differently. Do not infer pad
positions or add bridges from that generalization. Boards with separate C-/P-
charge/discharge ports require their own wiring plan; the K648 BAT connection
both charges and discharges, so do not join those ports by guesswork.

Remove the cell from the holder and disconnect every USB/external supply before
soldering or continuity work. Mount the PCM on insulated supports or secure it
against an insulating backing; no underside copper may touch perfboard buses,
fasteners or the cell holder. Solder flexible pigtails to its labelled pads and
land them on rated terminals, with strain relief. Do not force stiff 1.5 mm2 wire
onto tiny PCM pads or carry pack current through small perfboard pad chains.
Do not solder directly to the cell. Leave the fuse/cell out until wiring checks
are complete, and follow the board maker's connection/activation sequence.

The holder must fit the actual cell and have documented contacts/leads adequate
for battery current. A holder is not protection. If using an already protected
cell or protected pack, use its documented terminals instead of blindly stacking
a second, uncoordinated PCM. The exact PCM model/pad labels and fuse part number
are needed before treating the wiring and surge tolerance as verified.

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

### Later runtime option: a 1S2P pack

After the single-cell system is stable, a matched two-cell parallel pack can give
**approximately twice the runtime** at the same load. Two 3000 mAh cells form a
nominal 6000 mAh pack at the same 3.6/3.7 V nominal and 4.20 V full-charge voltage,
not a higher-voltage pack. Actual runtime depends on usable capacity, cutoff,
temperature and conversion efficiency. Charging from the same limited current
will take roughly longer in proportion to capacity.

The electrical topology is parallel (positive to positive, negative to negative),
but this is not authorization to connect two loose cells or holders together.
Different cell voltages can cause large equalization current before the player
draws anything. A pack PCM does not necessarily interrupt circulating current
between parallel cells. Matching voltage alone also does not prove cell health.

For this build, prefer a professionally assembled **protected 1S2P pack** from a
reputable supplier, with matched chemistry/model/capacity/age/state of charge,
appropriate pack/inter-cell fault protection and documented output/charge ratings.
Use its fused protected output at the existing IP5310 BAT interface:

```text
Documented protected 1S2P pack + --> appropriately rated fuse --> IP5310 BAT+
Documented protected 1S2P pack - -----------------------------> IP5310 BAT-
```

Fuse placement/rating must follow the pack design; its internal protected cells
are not exposed in this connection drawing. Do not charge the cells independently
then reconnect them at different charge levels, hot-swap a cell, or use a two-cell
series holder. **Never feed a 2S pack (up to 8.4 V) into this 1S battery input.**

A parallel pack may reduce battery sag, but a 3 A pack PCM/fuse does not become a
6 A device: it still carries the combined pack current. The IP5310 output limit
also stays unchanged. Recheck charging compatibility, protection and mechanical
space before adoption. It is worth considering if measured one-cell runtime is
too short; it is not needed just to make an otherwise adequate one-cell design
more complicated. The current BOM and schematic remain a single-cell baseline.

## Motor: driven gate, low-side switching

```text
GPIO27 -- TC4420 INPUT (pin 2)
             |
           10k
             |
            GND

TC4420 pins 1 + 8 (VDD) -> LOAD_5V; pins 4 + 5 (GND) -> LOAD_GND
100 nF + 1 uF between TC4420 VDD and GND, close to pins
TC4420 OUTPUT pins 6 + 7 -- one 100 ohm -- IRLZ44N GATE
IRLZ44N GATE -- 10k ------- IRLZ44N SOURCE -> LOAD_GND
IRLZ44N DRAIN ------------ MOTOR -
LOAD_5V ----------------- MOTOR +

Flyback diode: ANODE -> drain/motor -; CATHODE (band) -> motor +/LOAD_5V
100 nF across motor terminals (brush suppression)
220 uF (>=10 V): + -> LOAD_5V; - -> LOAD_GND, NOT drain/motor -
```

### TC4420CPA DIP-8 pin bridges

This table and the AHCT125 table below use **top-view numbering**: markings facing
you, notch at the top, pin 1 upper-left, numbering counterclockwise. The solder-side
view is mirrored. Disconnect battery and USB and remove the ICs before soldering
socket links.

| Pin(s) | Connection |
|---|---|
| 1 + 8 | Join VDD pins; connect to LOAD_5V. |
| 4 + 5 | Join GND pins; connect to LOAD_GND. |
| 6 + 7 | Join OUTPUT pins; through one 100 ohm resistor to IRLZ44N gate. |
| 2 | INPUT from GPIO27, with a 10k resistor to LOAD_GND. |
| 3 | NC; leave unconnected. |

**All three duplicate pairs must be connected externally**, including both VDD
pins, as required by the
[Microchip datasheet, page 1](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21419D.pdf#page=1).
These are three separate groups; never bridge supply, ground and output together.
Use short wire links between perfboard/socket pads, or a short common rail for
each group, not large solder blobs. Do not bridge intervening pins. Keep the
100 nF and 1 uF bypass capacitors in parallel between the joined VDD pins 1/8 and
GND pins 4/5, close to the driver.

TC4420 accepts TTL-style input at a 5 V supply; its minimum supply is 4.5 V.
Gate/input pulldowns are mandatory even with brownout enabled. Check on-state
drain voltage and temperature under the actual belt load. Protect against exposed
MOSFET tab contact: the IRLZ44N tab is electrically connected to drain.

## DIP AHCT125: LED and amplifier

Pin numbers below are for SN74AHCT125N DIP-14, top-view numbering verified against
the [TI datasheet, page 3](https://www.ti.com/lit/ds/symlink/sn74ahct125.pdf#page=3):

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

For this two-channel build, the socket bridge groups are:

- **1, 4, 7, 9, 12 -> LOAD_GND.**
- **10, 13, 14 -> LOAD_5V.**

Keep these two groups separate. Short links to the same local rail count as
connections; separate pin-to-pin jumpers are not needed when the rail already
joins the pins. Inputs 2 and 5 retain their separate 10k pulldowns, not direct
ground bridges. **Never join separate channel outputs 3, 6, 8 or 11 together.**
Unused outputs 8 and 11 are each left open, not bridged to each other or a rail.

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

The speaker is confirmed as **mono, 4 ohm, 3 W**, a suitable nominal load for the
MAX98357A. The 3 W marking is a power-handling rating, not a constant electrical
load. Start at low volume and avoid sustained clipping/full-scale test tones;
the amplifier can approach or exceed that nominal power under some conditions.
Its wattage label is not a speaker-protection limiter. Neither speaker lead goes
to GND, including an oscilloscope ground clip.

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

### Capacitor placement on module pins

Yes: a capacitor directly across a module's correct supply and GND pads is useful
when it keeps the leads short. Put the 100 nF ceramic closest to the supply entry
or IC supply pins; place bulk electrolytics nearby and mechanically support them.
On socketed ICs, the bypass can be soldered to the perfboard/socket supply pads
on the underside, clear of adjacent pins and insulated against accidental contact.
For AHCT125, it goes between pins 14 and 7. For TC4420CPA, put both the 100 nF
and 1 uF ceramics across joined VDD pins 1/8 and joined GND pins 4/5. The capacitors
connect between these groups; do not directly short the supply and ground groups.

Do not put capacitors in series with a supply, across arbitrary signal pins, or
between either amplifier speaker output and GND. The motor's small brush capacitor
is the deliberate exception across motor terminals; its bulk capacitor remains
between the motor's supply and GND, not across the switching drain. Check existing
module capacitors before adding more bulk. Every added bulk capacitor also adds
startup charging current, so record the fitted values in the manual test results.

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