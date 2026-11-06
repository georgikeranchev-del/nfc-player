# Manual Hardware Tests

This document owns the manual bring-up, motor, idle/sleep and USB/charging tests.
For intended behavior and timing, see [POWER_SEQUENCE.md](POWER_SEQUENCE.md).
Use [PINOUT.md](PINOUT.md) and [WIRING.md](WIRING.md) for connections and assembly
requirements; battery selection guidance remains in the wiring guide.

## Manual test preparation

**Status: procedure only; no hardware pass is recorded.** Firmware-driven tests
also require a successful real ESP32 build/upload; native tests do not establish
that prerequisite. Work through the battery-only tests before any charging tests.

- Follow [WIRING.md](WIRING.md): protected, fused cell; rated power wiring; correct
   flyback diode, gate driver, pulldowns and bypass capacitors. Brownout stays enabled.
   Never reconnect the earlier boost that reportedly produced 8-9 V.
- For battery-only tests, disconnect BOTH USB cables. Upload each test image with
   the ESP32 fully disconnected from the player harness and powered by its USB
   alone; remove USB before reconnecting the harness. Do not add USB just for logs.
- Secure the motor/platter and keep a reachable load switch. Rewire only with
   power removed. Do not deliberately stall the motor, short the battery or bypass
   protection. Stop promptly for failure to accelerate, repeated resets, unexpected
   heating, smell, damaged insulation or a cell outside its specified limits.
- Use a multimeter for battery/steady voltages and a scope for LOAD_5V at the
   DevKit/driver and ESP32 3.3 V during startup and audio onset. A meter's MIN/MAX
   alone does not prove short dips or overshoot are absent. Record unavailable
   measurements as **not checked**, not passed.
- Scope ground clips go ONLY to protected system GND, never raw cell negative,
   motor drain or either speaker lead. Use an isolated meter/differential probe
   for measurements across other nodes; do not bridge battery protection with
   grounded instruments. A current probe is preferable for peaks. A suitably rated,
   fused inline meter can measure average current but adds voltage drop; never put
   a meter in current mode across the cell or supply.
- Before testing, record cell model, charge/discharge limits, lower operating
   voltage limit and protection/fuse ratings. The lower operating limit must leave
   margin above the cell's minimum and protection cutoff. Stop discharge there;
   a protection trip is a fault, not the intended everyday endpoint.

Once the rails have risen, use **4.75-5.25 V on LOAD_5V and 3.14-3.47 V on 3.3 V**
as conservative bench screening targets during operation. Check the actual
components' limits too; tighter limits take precedence. These targets are not
absolute-maximum ratings or electrical certification. In particular, TC4420 is
not specified below 4.5 V. Investigate an out-of-band trace even without a reset.

For each run record: test ID, firmware/config, ramp duration, cell voltage before
and during load, belt/load, volume/LED settings, rail minima/maxima, battery peak
and average current (or measurement unavailable), time to first motion, time to
audio, resets/dropouts, temperatures, and pass/fail/not checked. Use the same
initial cell voltage as closely as practical when comparing candidates. Let the
motor stop completely between starts and allow components to cool as needed.

## Inrush current with a shunt and oscilloscope

**Before connecting probes:** identify the scope and probe models, whether inputs
share protective-earth ground, the probe attenuation settings, input voltage
limits and available sensitivity/bandwidth. If these are unfamiliar, leave the
shunt test disconnected for now. Learn the controls and probe compensation with
the scope's own documented calibration output and ground terminal first, using
the probe/manual's instructions; the player and battery stay disconnected.
Do not assume a handheld or USB scope has isolated channels.

Start by measuring **total battery current** during battery-only operation. This
shows what the cell/protection supplies, including boost losses. It is NOT the
same as 5 V output current or instantaneous motor winding current. Disconnect
both player USB cables and any external supply/programmer before this setup.

Do not intentionally exceed an unverified 3 A PCM/fuse to find its limit. Obtain
the fuse time-current curve and the PCM's continuous, surge, trip-delay and reset
specifications first. See [WIRING.md](WIRING.md#fuse-and-pcm-during-startup). A meter
in voltage mode can help with initial steady-voltage checks, but cannot certify
the inrush pulse. Never put a meter in current mode across a cell or supply.

### Shunt specification

Use **0.010 ohm (10 milliohm, often marked R010), at least 3 W, 1% tolerance**,
preferably a low-inductance metal-element current-sense resistor with a temperature
coefficient of 100 ppm/degree C or better. A through-hole part or a ready-made
four-terminal shunt fixture suits the perfboard bench setup. Check its actual
power derating, pulse-energy rating and mounting requirements. This is a purchase
specification, not confirmation of a particular shop's stock.

Do not confuse R010 with 10 ohm. Do not use an unidentified wirewound/cement power
resistor for accurate PWM peak measurements: inductance adds voltage unrelated
to resistive current. Use separate, short sense contacts directly at the element
terminals (Kelvin connections), not at the ends of its power cables or screw
contacts. Mechanically support and insulate the temporary fixture.

$$
V_{shunt}=I R_{shunt},\qquad P_{shunt}=I_{RMS}^{2}R_{shunt}
$$

| Current example | Voltage across 0.010 ohm | Dissipation at steady current |
|---|---|---|
| 1 A | 10 mV | 0.01 W |
| 3 A | 30 mV | 0.09 W |
| 5 A | 50 mV | 0.25 W |
| 10 A | 100 mV | 1.00 W |

These examples do not authorize a 10 A test or establish the cell/module ratings.
The 3 W choice provides thermal margin at the expected few-amp load; it is not
a fuse or current limiter. For pulses, also check energy against the resistor
datasheet. Even 50-100 mV of added drop can affect a marginal startup. Compare
operation with the shunt removed, reconnecting only with all power off. An
ordinary two-wire multimeter resistance reading is unreliable at 10 milliohm;
use the specified tolerance or verify with a known safe current and voltage.

### Battery-side connection for a grounded scope

The following assumes the TPS61088 module has a verified common IN-/OUT- return.
Check that with all power disconnected; do not bridge unknown terminals. IP5310
OUT is unused. For an externally protected pack, P- below is the
PCM's protected output, NOT raw cell negative/B-. For a protected battery pack,
use its documented protected output terminals. Keep the existing fuse/protection.

```text
Fused/protected 1S source P+ --+-----------------------> IP5310 BAT+
                             +-- SW_LOAD --> SW_BAT+ -> TPS61088 IN+ and divider top

Protected P- ----o----[ R_SHUNT = 0.010 ohm ]----o------> IP5310 BAT-
                |                              |         TPS61088 IN-/OUT-/system GND
            CH1 probe tip                 ALL scope ground clips
                                          All player grounds stay here

TPS61088 OUT+ --> LOAD_5V -----------------> CH2 probe tip (at ESP32 VIN)

Discharge return current flows through the shunt from right to left.
```

**CH1 reads negative while the battery discharges.** With the connections above:

$$
I_{battery}=-\frac{V_{CH1}}{0.010\ \Omega}
$$

Thus -50 mV means +5 A battery discharge. Invert CH1 for a positive display, or
apply the minus sign in the calculation. CH2 directly reads the 5 V rail relative
to system GND. A third channel, if available, can monitor ESP32 3.3 V with its
ground at that same system-GND point.

All scope channel grounds are normally joined to each other and protective earth.
**Never put a second ground clip on P-, raw B-, a positive rail, motor drain, or
either speaker terminal.** A clip on P- would short this shunt through the scope.
Do not let a USB cable, bench supply, programmer or another instrument create a
parallel return. Do not defeat the scope's protective earth, and do not assume
a battery-powered/USB scope has individually isolated channels. Attach probes
with the circuit off. This setup is only for the isolated battery-only test.
The divider bottom and filter return stay on system GND, right of the shunt.
Remove the shunt before ADC calibration; its voltage drop changes the reading.

### Capture procedure

1. Complete the earlier wiring/protection checks and start at the appropriate
   B1-B5 stage below. Check shunt wiring and absence of an alternate return before
   powering. Use short power leads and short probe sense/ground connections.
2. Use DC coupling and a high-impedance scope input, not 50 ohm termination.
   A suitable 1x probe can improve millivolt sensitivity here; match the scope's
   attenuation setting to the probe. Start around 10-20 mV/div on CH1 and adjust
   to avoid clipping the trace. Record the zero-current offset before power-on.
3. Use single acquisition with 20-30% pre-trigger. On the uninverted CH1 trace,
   choose a falling trigger below the idle-current level. Start around 200-500
   ms/div to capture the ramp plus audio/LED onset; extend the record for a 2 s
   ramp. A sample rate of at least 1 MS/s is a useful long-record starting point
   if the scope has sufficient memory, not a guarantee of capturing fast peaks.
4. Capture cold boost startup, load-switch turn-on and tag-triggered motor start
   as separate events. Their capacitor/boost loads differ. Keep CH2 recording the
   rail sag at the same time. Follow the existing quiet/full-load ramp comparison;
   do not force a motor stall to obtain a larger reading.
5. Make a separate short, high-sample-rate acquisition to inspect 20 kHz PWM and
   switching peaks. A 20 MHz bandwidth limit can reduce pickup if available, but
   record the bandwidth/sample rate used. Long timebases, sample aliasing, probe
   loops and shunt inductance can hide or invent apparent peaks. Do not average
   away a one-time inrush event or interpret every narrow spike as real current.
6. Record peak and steady current, pulse duration, minimum 5 V/3.3 V, battery
   voltage, mechanical load and capacitor values. Stop for the existing fault
   conditions. Disconnect power before removing the shunt or changing probes.

For reference on common-ground limitations, see
[Tektronix: floating measurements and operator protection](https://www.tek.com/en/documents/technical-brief/floating-oscilloscope-measurements-and-operator-protection).
Exact trigger/menu settings depend on the scope and probe model.

### Other current locations

To measure the boost's **5 V load current**, a shunt in the positive feed between
TPS61088 OUT+ and LOAD_5V star preserves the common ground. That requires a
proper differential measurement or suitable current probe; do not clip a normal
scope ground to either high-side shunt terminal. Differential probes must resolve
millivolts on a roughly 5 V common-mode signal; many high-voltage probes are too
noisy or attenuate too much. Two ordinary channels with subtraction are not an
accurate default for such a small voltage difference.

A shunt in the motor's supply branch measures branch supply current, not all
recirculating winding current during PWM off-time. Do not insert the first test
shunt between the MOSFET source and its driver/ESP32 ground: that changes the gate
reference. Keep the low-side battery setup out of the later charging/dual-USB
tests; those need a separately reviewed measurement connection.

## Perfboard assembly and acceptance

1. Place modules/socket footprints on a 200 x 150 mm cardboard layout including
   mounting holes, motor/belt plane, speaker, battery and wire clearance. Keep
   PN532 antenna away from metal/battery/motor; test tag reading through the lid.
2. Build only protected pack/IP5310/TPS61088/switch/star distribution first. Check
   polarity, shorts and unloaded output before fitting expensive boards.
3. Test TPS61088 with a known dummy load and SW_LOAD. Stable 5 V must exist without any
   ESP32 firmware. Reject excess voltage, unexpected cycling or heat.
4. Verify the divider with a meter before connecting GPIO34. Socket and add ESP32,
   then SD/PN532, then AHCT/LED/amp with motor disconnected; calibrate ADC below.
   Never change wiring live. For programming disconnect the player harness from
   ESP32 and use its USB alone, then remove USB before reconnecting battery wiring.
5. Add driver/flyback/motor last. First test without belt, then with the final
   platter. Keep amp muted/LEDs black during the ramp as firmware does.
6. Confirm every reset leaves gate LOW and amp off. Measure 5 V,
   3.3 V, battery current and motor voltage at start. No extended stall tests.
7. Measure normal sleep current and encoder wake, plus low-battery sleep without
   button wake. Confirm SW_LOAD removes boost AND divider power; no KEY driver remains.
8. Put strain relief, insulation and standoffs in before enclosure testing. Leave
   access to the fuse/switch and battery sense test points. Do not rely on loose
   breadboard contacts or perfboard pad islands for multi-amp return paths.

## Battery voltage calibration and cutoff

This section does two jobs: (1) make the voltage the firmware reports match your
multimeter, and (2) check that the low-battery cutoffs trigger. The pack is about
3.6 V nominal and 4.20 V full, like a single cell. Always calibrate against the
meter, never against "3.7 V" or a charge percentage. All procedures here are
unverified until you record results.

The firmware reports the voltage at **SW_BAT+**: the supply-side end of the 100k
resistor (the same net as the boost input). **GND** below means the ESP32 GND pin.

### What you need

| Item | Notes |
|---|---|
| Digital multimeter, DC volts | Needs 0.001 V resolution on a 20 V range. Its accuracy limits the calibration, so use the same meter for every reading. |
| Adjustable DC source, 3.0-4.3 V | A bench supply with current limit (set 50 mA) is best. Without one, a buck module (LM2596-type) powered from a USB 5 V charger works: trim its output with the meter BEFORE connecting anything. |
| ESP32 DevKit alone, USB cable, PC | Arduino IDE serial monitor at 115200 baud. |
| Divider parts | 100k 1%, 47k 1%, 100nF, plus a spare 100k (for GPIO35), clip leads and a breadboard. |
| Optional | A second meter to cross-check the first. |

### Part 1: bench calibration (DevKit alone, nothing else attached)

Using only the DevKit on its own USB cable avoids every power-path conflict: there
is no boost, no battery and no peripheral. Disconnect the player harness first.

```text
source + --[100k]--+--[47k]-- GND          source - ---- ESP32 GND
                   +--[100nF]-- GND
                   +--> D34 (GPIO34)
ESP32 3V3 --[100k]--> D35 (GPIO35, reads HIGH = no charger)
```

1. **Measure the resistors** out of circuit with the meter in resistance mode and
   write down both values (for example 99.6k and 47.2k).
2. **Set the source** to 4.00 V with the 50 mA limit, checking it with the meter
   across the source terminals. Never go above 4.3 V: the firmware treats
   anything above 4.35 V as a wiring fault.
3. **Check the divider before the ESP32 sees it.** Connect the source with D34 still
   disconnected. Meter red probe on the 100k/47k junction, black on GND. Expect
   `4.00 x R47 / (R100 + R47)`, about 1.279 V. If it is more than 1% off, fix the
   wiring before going on. The meter loads the junction slightly, so a reading
   about 0.3% low is normal.
4. **Connect the junction to D34**, plug in USB and upload the firmware. Open the
   serial monitor at 115200 baud. Every 5 s you will see a line such as
   `Battery ADC: 1275 mV; pack: 3975 mV; charger: absent`. A later "SD
   initialization failed" fault is expected: nothing else is attached, and battery
   monitoring keeps running.
5. **Record a table.** For each source setting of **4.10, 3.90, 3.70 and 3.50 V**:
   wait 20 s, then read the meter between the source + and GND (this is
   `V_meter`), and take the average of three consecutive `pack:` readings (this is
   `V_reported`). Stay at or above 3.50 V: at or below 3.30 V for 2 s the
   firmware correctly deep-sleeps (reset with the EN button to restart it).
6. **Compute the factor** for each row, starting from `BATTERY_CALIBRATION_PERMILLE
   = 1000`:

$$
C_{new}=\operatorname{round}\left(C_{old}\frac{V_{meter}}{V_{reported}}\right)
$$

   Example: meter 3900 mV, firmware 3750 mV, old factor 1000 gives **1040**. If the
   four factors differ by more than about 10, suspect poor clip contact, a noisy
   ground or a wrong resistor value. Use the 3.70 V row if the spread is small. A
   result outside 800-1200 means a wiring or value error, not a reason to relax
   the limit.
7. **Enter it** at [config.h](firmware/nfc_player/config.h#L23), upload again and
   repeat the check at 4.10, 3.70 and 3.50 V. Accept if the reported value is within
   about 30 mV of the meter. Also look at 3.35 V, close to the cutoff: about 50 mV
   there is a reasonable bench target. One factor cannot fix large nonlinearity.

### Part 2: hard cutoffs on the same rig

These work with the bare DevKit. The D35 pull-up must be fitted so the charger reads
absent. Start at 3.60 V and watch the serial log.

| Do this | Expect |
|---|---|
| Drop to 3.25 V and hold | After about 2 s: "Battery voltage unsafe ... waking for charger", then silence (deep sleep). |
| Raise to 3.80 V | It stays asleep. Only EN resets it. |
| Set 3.25 V, press EN | It logs the reading, then sleeps again without any SD message. |
| Run at 3.60 V, drop quickly to 2.9 V | Sleeps within one sample (about 0.25 s), no 2 s wait. |
| Set 4.40 V, press EN | Sleeps ("unsafe"): above 4.35 V is treated as a fault. |
| 3.60 V, drop to 3.25 V for 1 s, back to 3.60 V | Keeps running: a short dip must not trigger shutdown. |
| In low-battery sleep, touch D35 to GND briefly | The ESP32 wakes (this simulates plugging the charger in). Holding D35 low while still at 3.25 V makes it wake and re-sleep about every 30 s. |

### Part 3: low-battery lockout on the assembled player (no discharging)

You do not need to drain the cells. Temporarily move the thresholds so the real,
healthy pack counts as "low". Read the current pack voltage from the serial log (say
3.90 V) and change these in [config.h](firmware/nfc_player/config.h):

| Constant | Normal | Temporary test value |
|---|---|---|
| `BATTERY_START_MIN_MV` ([line 27](firmware/nfc_player/config.h#L27)) | 3500 | pack + 150 mV (4050) |
| `BATTERY_RECOVER_MV` ([line 33](firmware/nfc_player/config.h#L33)) | 3700 | start-min + 150 mV (4200), and below 4600 |

The compile-time checks keep the order CRITICAL < LOW < START_MIN < RECOVER < 4600, so
a bad combination will not build. Upload with the harness disconnected and only the
DevKit USB connected (the usual programming rule), remove USB, reconnect and test.
To test the hard cutoff instead, set `BATTERY_LOW_MV` (line 28) above the pack
voltage with START_MIN above it. **Restore all values to the normal column afterwards
and upload again.**

Checks:
- **Warning file:** put a short `/player/low_battery.mp3` on the SD card. Present a
  tag: the warning must play once at fixed volume 10 with motor and LEDs off, not
  the song. Remove and re-present for another warning; a tag left in place must not
  loop it. Removing the tag mid-warning must not cut it short. It stops by itself
  after 10 s; a missing or corrupt file stays silent and the song never plays.
- **Song cut-off:** start a song at a healthy voltage, then make the pack count as
  low for 5 s: playback must stop while the NFC reader stays active. A dip shorter
  than 5 s must not stop a playing song.
- **Latch:** a voltage rebound and a normal sleep/wake must not clear the lock.
- **Hard cutoff:** a held encoder button cannot prevent shutdown; the button cannot
  wake low-battery sleep; a boot at or below 3.30 V must not start SD, NFC or audio;
  between 3.30 V and 3.50 V only warning/idle is allowed. Normal idle sleep must
  still wake with the encoder.
- **Afterwards:** measure the remaining sleep current and open SW_LOAD. The
  boost and peripherals are still connected during software sleep.

### Charger recovery check

This needs the 2N7000 sense circuit fitted. With the player locked out (Part 3
values), a voltage rebound alone must not clear the lock. Plug in the charger and
watch the log: it must show `charger: present`, and the lock clears only when the
pack stays at or above `BATTERY_RECOVER_MV` for 30 s. Unplugging, or dropping below
that value, restarts the 30 s timer. How far the voltage rises under charge depends
on the charge current, so choose the test value after watching the `pack:` log while
charging. Also confirm that GPIO35 reads LOW with USB present and HIGH without, that
hard low-battery sleep wakes on plug-in (and re-checks every 30 s if USB is already
present), that the 2N7000 gate sits at about 4.5 V with USB present, and that
nothing above 3.3 V ever reaches the GPIO. Keep the PCM independent and validate its
documented limits separately.

## Choosing the motor ramp duration

The current `MOTOR_RAMP_MS` in
[firmware/nfc_player/config.h](firmware/nfc_player/config.h) is **600**.
Use **1000 ms as the first bench trial**, then compare 600 and 2000 ms under the
same conditions. These are test candidates, not validated settings; the firmware
default remains unchanged until measured. Choose the shortest ramp that starts
reliably with the real belt/platter and at the lowest intended battery level.

The ramp starts at duty 60/255, not zero, and ends at 230/255. It limits neither
instantaneous motor current nor battery current. Extending it can reduce the
acceleration load, but can also prolong a motionless, low-torque start. A motor
that needs a push or only buzzes has not passed. Do not solve a stall or collapsing
supply by repeatedly increasing the delay.

`Motor.ready()` means the duty ramp has finished, NOT that speed was measured.
Record when rotation begins and whether acceleration is substantially complete
before audio/LEDs start. Hold start/run duty, PWM frequency, track, volume, LED
settings and mechanical load constant when comparing ramp durations.

Only change `MOTOR_RAMP_MS` for that comparison; `AUDIO_FADE_MS` and `LED_START_MS`
remain 1000 ms and begin afterward. With a tag already present, a 1 s ramp permits
first audio around 1.6 s after `setup()`, versus 2.6 s for a 2 s ramp, plus library
and NFC delays. A 2 s motor ramp therefore cannot meet a 2 s total startup target.

## Battery-only tests

| ID | Action | Required observation |
|---|---|---|
| B1 | Player disconnected: test protected pack/TPS61088 with SW_LOAD and a known, rated dummy load, increasing only within the module/pack/wiring ratings. | Clean startup and stable 5 V at intended load and operating input range; no excessive voltage, cycling or heating. |
| B2 | Connect ESP32, calibrated divider, SD, PN532, buffers, amp and ring; leave motor disconnected. Cold-start without a tag and remain awake/idle for at least 2 minutes. | Motor command off, amp muted, no resets; TPS61088 sustains the rail without KEY pulses. All connected GPIO peripherals remain powered. |
| B3 | Power off, connect motor with belt removed. Trial 1000 ms ramp using a known tag/MP3 and the quiet settings below; make five starts from rest. | Repeatable unassisted rotation, acceptable rail/current traces, no abnormal heat. Remove the tag during a ramp to confirm drive is cancelled. |
| B4 | Fit the real belt/platter. Compare 600, 1000 and, only if useful, 2000 ms; make five starts per candidate with the quiet settings and comparable cell voltage. Skip a candidate after an unsafe result. | Record breakaway and acceleration, not just absence of brownout. Select the shortest reliable ramp with acceptable supply margin; do not hand-start the platter. |
| B5 | Restore normal audio/LED settings. Test the selected ramp at the loudest intended volume and brightest enabled pattern, with a full cell and again near the chosen lower operating limit. Make ten starts at each condition. | All starts succeed; motor load plus audio/LED fade causes no reset, audio dropout, LED corruption, rail violation or protection trip. Record battery sag and component temperatures. |
| B6 | Run representative playback for 10 minutes, extending to 30 minutes only if stable; exercise all three repeats, tag changes, removal during ramp and missing MP3. | Replays remain reliable; temperature stays within component/cell limits without continuing abnormal rise. No tag or missing MP3 means motor off. For approximate speed, ten platter turns take about 18 s. |
| B7 | Test 1.5 s hold/release sleep and 5-minute idle sleep. Observe rails/current for at least 60 s, then wake by encoder with a healthy pack. | Motor/amp off, LEDs black; normal encoder wake works while powered. Measure remaining draw; do not assume TPS61088 turns off. SW_LOAD must remove boost and divider input together. |
| B8 | Perform the battery ADC calibration and cutoff procedure above with a substitute supply, not by over-discharging cells. | Verified voltage accuracy, startup refusal, dip filtering, shutdown and no button wake in low-battery sleep. Record residual current and switch off afterward. |
| B9 | Perform the soft inhibition/warning test above. | One bounded warning per new request; no motor, song replay or voltage-rebound unlock. Missing warning remains fail-closed. |
| B10 | Move the existing button from IP5310 KEY to GPIO39 and LOAD_GND with a 10k pull-up to 3V3, remove the transistor circuit from GPIO17, and put `/player/help.mp3` on the SD card. Press while idle, during help, during a song, with a tag present, and with the file removed. Hold it while powering on. | Help plays at the set volume with the motor off; a second press stops it; a tag stops it and starts that song; presses during songs do nothing; a missing file only logs; a button held at power-on does nothing until released. Check GPIO39 reads HIGH idle and LOW pressed, and that there is no 5 V on it. |

For B3/B4 only, set `INITIAL_VOLUME = 0`, `LED_BRIGHTNESS = 0` AND
`LED_START_BRIGHTNESS = 0` in the test image. Both LED values must be zero to pass
the configuration check. Leave peripheral supplies connected: these settings
suppress sound/light, not electronics' idle current. Keep the normal SD/NFC/MP3
path so the real motor algorithm runs. Restore the original settings for B5.

During B3-B5 capture both the motor start and the later audio/LED onset, including
voltage sag at the converter input. Repeat any configuration change with the
isolated USB upload procedure above. Never run a hard full-duty stall as a
comparison test.

If the quiet loaded test fails, investigate friction, motor drive, battery sag,
connections and TPS61088 capacity before adding audio load. If only the later fade
fails, investigate total power demand and whether motor acceleration is still in
progress. A 2 s ramp is useful only when measurements show a benefit. More mAh or
a longer delay cannot fix an inadequate continuous-current supply.

## Platter speed check

With the final belt/platter fitted, count 10 revolutions (~18 s for 33 1/3 RPM).
Adjust `MOTOR_RUN_DUTY` in
[firmware/nfc_player/config.h](firmware/nfc_player/config.h) by about 5 counts per
trial. Full-duty too slow means revise friction/ratio rather than increasing
voltage. Recheck loaded soft-start after changing the run duty.

## Later USB and charging tests

These tests are gated, not permission to connect two USB sources to an unverified
power path. Keep battery-only as the accepted configuration until the applicable
stage passes. Read [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) before changing it.

| ID | Prerequisite and action | Required observation |
|---|---|---|
| U1: charge only | Player disconnected/load switch open; DevKit USB absent. Verify the K648 charge voltage/current against the chosen cell, then use a suitable regulated 5 V USB source on K648 only. Observe charge completion and plug/unplug. | Correct cell voltage/current and termination; acceptable cell/module temperatures. The battery can charge with the load switch open. A battery-disconnect switch is a different case. |
| U2: play while charging | B1-B9 and U1 passed; DevKit USB still absent. Start with idle, then selected-ramp playback. Connect/disconnect K648 charging USB during idle, ramp and steady playback, five times per state. | Rail targets maintained, no resets/dropouts or reverse power into the charger. At a partly discharged cell, measure whether net battery current actually charges it under the intended load; a charge LED is not proof. Recheck full-charge termination with the player running. |
| U3: isolated programming while charging | U1 passed. Open the load switch and physically disconnect the complete player harness, including GPIOs, from ESP32. Charge the cell through K648 USB while programming ESP32 through its own PC USB. | Programming succeeds and charging remains correct. The two 5 V rails are not joined through ESP32; this tests simultaneous charging/programming, not playback. Disconnect PC USB before restoring the harness. |
| U4: both USB cables, fully wired | NOT READY until the actual DevKit and K648 power paths are documented and an appropriate power-mux/reverse-blocking or verified data-only arrangement is implemented. Account for GPIO back-power and USB bridge power/detection too. Verify with protected/current-limited sources and dummy loads before using the PC. | No uncontrolled source-to-source current or powered-off peripheral back-feed in any permitted cable/switch state; all source-current limits respected. One series diode, a common ground, or an unspecified USB isolator does not establish this. |
| U5: integrated transitions | U4 passed. Test charger-first and PC-first connection order, removal of each cable, reset and upload, first idle then during playback. Repeat five times and observe both rails, source currents and safe outputs. | Programming resets are expected: motor off and amp muted during reset. No stuck-on drive, unsafe transient or unintended USB powering of the player. Resume playback only through the normal tag/start sequence. |

For U2, distinguish **plays while USB is attached** from **charges while playing**.
Input power must cover the player, charging and conversion losses. A weak source,
poor cable or unsuitable module power path can leave the cell discharging despite
a charging indicator. Near full charge, tapering to zero is normal; assess net
charging with a partly discharged cell as well. Do not infer pass-through quality
or safe dual-USB operation from the IP5310 chip name alone.