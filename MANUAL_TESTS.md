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

The following assumes the K648 has a verified common BAT-/OUT- return, as in the
project wiring. Check that with all power disconnected; if it is not common, do
not bridge unknown terminals. For an externally protected cell, P- below is the
PCM's protected output, NOT raw cell negative/B-. For a protected battery pack,
use its documented protected output terminals. Keep the existing fuse/protection.

```text
Fused/protected 1S source P+ --------------------------> IP5310 BAT+

Protected P- ----o----[ R_SHUNT = 0.010 ohm ]----o------> IP5310 BAT-
                |                              |         and OUT-/system GND
            CH1 probe tip                 ALL scope ground clips
                                          All player grounds stay here

IP5310 OUT+ --> load switch --> LOAD_5V ----> CH2 probe tip (at ESP32 VIN)

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
the load switch and LOAD_5V star preserves the common ground. That requires a
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
2. Build only protected battery/IP5310/switch/star distribution first. Check
   polarity, shorts and unloaded output before fitting expensive boards.
3. Test boost with a known dummy load and KEY. Stable 5 V must exist without any
   ESP32 firmware. Reject excess voltage, unexpected cycling or heat.
4. Socket and add ESP32, then SD/PN532, then AHCT/LED/amp with motor disconnected.
   Never change wiring live. For programming disconnect the player harness from
   ESP32 and use its USB alone, then remove USB before reconnecting battery wiring.
5. Add driver/flyback/motor last. First test without belt, then with the final
   platter. Keep amp muted/LEDs black during the ramp as firmware does.
6. Confirm every reset leaves gate LOW, amp off and KEY released. Measure 5 V,
   3.3 V, battery current and motor voltage at start. No extended stall tests.
7. Measure sleep current and wake behavior both before and after actual IP5310
   shutdown. Only then choose one-control hardware or fit the fallback KEY button.
8. Put strain relief, insulation and standoffs in before enclosure testing. Leave
   a service connector for KEY and access to the fuse/switch. Do not rely on loose
   breadboard contacts or perfboard pad islands for multi-amp return paths.

## KEY characterization

Measure KEY open voltage and tap/hold/double-tap behavior with a high-impedance
meter/scope. Do not measure pull-up current by putting an ammeter straight across
unidentified terminals. For a source-resistance estimate use a known high-value
resistor, reduce cautiously only after verifying voltages and dissipation.

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
| B1 | Player disconnected: test protected cell/IP5310 with KEY and a known, rated dummy load, increasing only within the module/cell/wiring ratings. | Clean startup and stable output at the intended load; no excessive voltage, cycling or heating. Characterize no-load auto-off separately. |
| B2 | Connect ESP32, SD, PN532, buffers, amp and ring; leave motor disconnected. Cold-start without a tag and remain awake/idle for at least 2 minutes. | Motor command off, amp muted, no resets; KEY keep-alive prevents the previously observed approximately 30 s idle dropout while awake. All connected GPIO peripherals remain powered. |
| B3 | Power off, connect motor with belt removed. Trial 1000 ms ramp using a known tag/MP3 and the quiet settings below; make five starts from rest. | Repeatable unassisted rotation, acceptable rail/current traces, no abnormal heat. Remove the tag during a ramp to confirm drive is cancelled. |
| B4 | Fit the real belt/platter. Compare 600, 1000 and, only if useful, 2000 ms; make five starts per candidate with the quiet settings and comparable cell voltage. Skip a candidate after an unsafe result. | Record breakaway and acceleration, not just absence of brownout. Select the shortest reliable ramp with acceptable supply margin; do not hand-start the platter. |
| B5 | Restore normal audio/LED settings. Test the selected ramp at the loudest intended volume and brightest enabled pattern, with a full cell and again near the chosen lower operating limit. Make ten starts at each condition. | All starts succeed; motor load plus audio/LED fade causes no reset, audio dropout, LED corruption, rail violation or protection trip. Record battery sag and component temperatures. |
| B6 | Run representative playback for 10 minutes, extending to 30 minutes only if stable; exercise all three repeats, tag changes, removal during ramp and missing MP3. | Replays remain reliable; temperature stays within component/cell limits without continuing abnormal rise. No tag or missing MP3 means motor off. For approximate speed, ten platter turns take about 18 s. |
| B7 | Test 1.5 s hold/release sleep and 5-minute idle sleep. Wait at least 60 s after sleeping and monitor the rails, extending observation if auto-off is not yet clear. | Motor/amp off, KEY released; measure actual standby current. Encoder wake works only while rails remain valid. If boost shuts down, KEY restores a clean cold boot; record lingering low voltage or reset cycling as a problem. |

For B3/B4 only, set `INITIAL_VOLUME = 0`, `LED_BRIGHTNESS = 0` AND
`LED_START_BRIGHTNESS = 0` in the test image. Both LED values must be zero to pass
the configuration check. Leave peripheral supplies connected: these settings
suppress sound/light, not electronics' idle current. Keep the normal SD/NFC/MP3
path so the real motor algorithm runs. Restore the original settings for B5.

During B3-B5 capture both the motor start and the later audio/LED onset. Also check
KEY release during ramp/playback: approximately 120 ms LOW every 20 s while awake,
not a LOW stretched by the main loop. Repeat any configuration change with the
isolated USB upload procedure above. Never run a hard full-duty stall as a
comparison test.

If the quiet loaded test fails, investigate friction, motor drive, battery sag,
connections and IP5310 capacity before adding audio load. If only the later fade
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
| U2: play while charging | B1-B7 and U1 passed; DevKit USB still absent. Start with idle, then selected-ramp playback. Connect/disconnect K648 charging USB during idle, ramp and steady playback, five times per state. | Rail targets maintained, no resets/dropouts or reverse power into the charger. At a partly discharged cell, measure whether net battery current actually charges it under the intended load; a charge LED is not proof. Recheck full-charge termination with the player running. |
| U3: isolated programming while charging | U1 passed. Open the load switch and physically disconnect the complete player harness, including GPIOs, from ESP32. Charge the cell through K648 USB while programming ESP32 through its own PC USB. | Programming succeeds and charging remains correct. The two 5 V rails are not joined through ESP32; this tests simultaneous charging/programming, not playback. Disconnect PC USB before restoring the harness. |
| U4: both USB cables, fully wired | NOT READY until the actual DevKit and K648 power paths are documented and an appropriate power-mux/reverse-blocking or verified data-only arrangement is implemented. Account for GPIO back-power and USB bridge power/detection too. Verify with protected/current-limited sources and dummy loads before using the PC. | No uncontrolled source-to-source current or powered-off peripheral back-feed in any permitted cable/switch state; all source-current limits respected. One series diode, a common ground, or an unspecified USB isolator does not establish this. |
| U5: integrated transitions | U4 passed. Test charger-first and PC-first connection order, removal of each cable, reset and upload, first idle then during playback. Repeat five times and observe both rails, source currents and safe outputs. | Programming resets are expected: motor off, amp muted and KEY released during reset. No stuck-on drive, unsafe transient or unintended USB powering of the player. Resume playback only through the normal tag/start sequence. |

For U2, distinguish **plays while USB is attached** from **charges while playing**.
Input power must cover the player, charging and conversion losses. A weak source,
poor cable or unsuitable module power path can leave the cell discharging despite
a charging indicator. Near full charge, tapering to zero is normal; assess net
charging with a partly discharged cell as well. Do not infer pass-through quality
or safe dual-USB operation from the IP5310 chip name alone.