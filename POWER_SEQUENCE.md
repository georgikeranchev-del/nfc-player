# Power Sequence

## Start and wake paths

**Load switch open:** close SW_LOAD to power TPS61088 and the battery divider
from protected P+. A stable 5 V rail cold-boots the ESP32. The IP5310 is charger
only; its OUT pins and KEY are not in the player power/wake path. Capacitor/boost
startup and the ESP32 ROM occur BEFORE the timing below.

**5 V and 3.3 V still valid, ESP32 in deep sleep:** press encoder SW on GPIO4 to
GND. EXT0 wakes/reset-starts the ESP32; `setup()` and all boot stages run again.
This wake is allowed only after normal user/idle sleep.

**Low-battery sleep:** button and timer wake are disabled. Open SW_LOAD, recharge,
then switch on again. Charging alone does not wake the ESP32. Boot requires a
calibrated reading of at least 3.50 V; a reading above 4.35 V is rejected too.
PCM recovery, if it tripped, follows the PCM maker's procedure, not GPIO/KEY taps.

## Boot stages

Times are earliest offsets from entry to `setup()`, not from the finger press.
Each peripheral stage runs once; actual library calls can exceed these offsets.

| Earliest time | Stage | Loads |
|---|---|---|
| 0 ms onward | Safe motor/amp outputs, reset log, ADC setup and 20 ms settling, initial voltage sample, encoder init, LED black, PWM zero | Unsafe voltage enters low-battery sleep before SD/NFC/audio initialization |
| 200 ms | Initialize SD on HSPI, check/create track directory | Motor/amp still off |
| 400 ms | Initialize PN532, configure finite retries, start reader task | Motor/amp still off |
| 600 ms | Allocate/configure audio output silently; ready/standby LEDs | No I2S playback until tag |

POWER and GPIO outputs get initialized at entry, before the SD/NFC delays.
These are not switched supply rails: PN532/SD/amp/LED electronics already have
power as soon as LOAD_5V appears. LEDs are cleared early to avoid keeping a prior
latched bright pattern through an ESP32-only reset.

There are no startup or keep-alive KEY taps. GPIO17 is unused. ADC1 GPIO34 is
sampled every 250 ms (16 readings averaged); calibrated pack voltage is logged
at boot and every 5 s while awake. Wiring and calibration must be completed
before relying on the thresholds; a missing/zero reading prevents startup.

## Tag-to-play sequence

For a tag detected at/after ready, call that instant T:

| Time | Event |
|---|---|
| T | Stop old track, check path, LED black, start motor duty 60/255 |
| T to T+600 ms | Motor duty rises non-blockingly to 230/255; audio remains muted |
| T+600 ms or later | Open/begin MP3, gain zero, enable amp, start LED fill/brightness ramp |
| Next 1000 ms | Audio gain fades to selected volume and LEDs reach normal brightness |

If a tag is already on the reader, nominal first audio can be around 1.2 s after
`setup()`, with full volume around 2.2 s, plus any SD/PN532/decoder delays. NFC
scan latency is additional. Measure before promising a strict two-second start.
No tag means idle: neither motor nor audio runs at boot. Removing the tag during
the ramp cancels it; changing tag replaces it. Three plays are followed by idle,
and re-presenting the tag is required to start again. Replays also ramp/fade.

## Sleep sequence

Hold encoder for 1.5 s AFTER debounce (40 ms). The controller
stops motor/audio, clears LEDs and asks the NFC task to stop polling. It waits
non-blockingly for debounced release, then arms GPIO4 LOW wake and deep-sleeps.
A button held during boot/wake is ignored until released so it cannot immediately
put the player back to sleep. Five minutes of idle causes the same shutdown.

While awake, including an initialization fault or waiting for button release,
voltage at/below 3.30 V continuously for 2 s triggers low-battery sleep. At/below
3.00 V or above 4.35 V triggers it on the next sample. Recovery above 3.30 V
before the 2 s timeout cancels that pending low-voltage event. Once sleeping,
voltage rebound cannot restart playback. Low-battery shutdown does not wait for
the encoder to be released and does not enable its wake source.

NFC polling stop is not PN532 power-down; TPS61088, SD, PN532, ring idle logic,
buffers, regulator and gate driver can still draw current. ADC monitoring stops
in deep sleep. **Sleep is not a battery disconnect.** Measure sleep current,
open SW_LOAD for storage/recharge, and retain suitable PCM protection. A hardware
low-voltage disconnect would be needed for autonomous isolation. Cooperative
sampling can be delayed by library calls and does not replace that protection.

## Manual tests

Test preparation, motor-ramp comparisons, battery-only and no-tag idle/sleep
checks, and later USB/charging validation are in [MANUAL_TESTS.md](MANUAL_TESTS.md).
This document describes the intended sequence; test procedures and acceptance
criteria are maintained in that dedicated guide.