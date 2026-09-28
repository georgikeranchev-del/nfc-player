# Power Sequence

## Two different wake paths

**IP5310 output really off:** a physical KEY/GND tap wakes the boost. If its output
is stable and the load switch is closed, ESP32 VIN rises and the DevKit cold-boots.
ESP32 software cannot create this first tap while unpowered. Capacitor/boost
startup and the ESP32 ROM occur BEFORE the timing below.

**5 V and 3.3 V still valid, ESP32 in deep sleep:** press encoder SW on GPIO4 to
GND. EXT0 wakes/reset-starts the ESP32; `setup()` and all boot stages run again.
Pressing KEY alone does not inherently wake a still-powered sleeping ESP32.

**Battery/load switch open:** close it first. KEY cannot bypass an open switch.
If the boost has auto-shut, a KEY tap may also be necessary.

## Boot stages

Times are earliest offsets from entry to `setup()`, not from the finger press.
Each peripheral stage runs once; actual library calls can exceed these offsets.

| Earliest time | Stage | Loads |
|---|---|---|
| 0 ms | Safe motor/amp/KEY outputs, reset log, encoder init, LED black, PWM attached at zero | Motor off, amp shutdown, LEDs black |
| 200 ms | Initialize SD on HSPI, check/create track directory | Motor/amp still off |
| 400 ms | Initialize PN532, configure finite retries, start reader task | Motor/amp still off |
| 600 ms | Allocate/configure audio output silently; ready/standby LEDs | No I2S playback until tag |

POWER and GPIO outputs get initialized at entry, before the SD/NFC delays.
These are not switched supply rails: PN532/SD/amp/LED electronics already have
power as soon as LOAD_5V appears. LEDs are cleared early to avoid keeping a prior
latched bright pattern through an ESP32-only reset.

There is NO automatic startup KEY tap: the user already generated the cold-start
tap, and an immediate extra pulse could be interpreted as a double press. While
awake, the first configured keep-alive request is after 20 s, then every 20 s,
with a nominal 120 ms LOW on KEY via the transistor. Verify those values on K648.

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

Hold encoder for 1.5 s AFTER debounce (40 ms). The controller disables KEY pulses,
stops motor/audio, clears LEDs and asks the NFC task to stop polling. It waits
non-blockingly for debounced release, then arms GPIO4 LOW wake and deep-sleeps.
A button held during boot/wake is ignored until released so it cannot immediately
put the player back to sleep. Five minutes of idle causes the same shutdown.

NFC polling stop is not PN532 power-down; SD, PN532, ring idle logic, buffers,
regulator and gate driver still draw current. Measure whether the IP5310 actually
switches off. If it stays on, encoder wake works but battery consumption remains.
If it turns off, encoder GPIO4 alone cannot wake it; use the KEY cold-start path.

## Bench acceptance

1. Scope LOAD_5V and ESP32 3.3 V at battery start, motor start and sleep/wake.
   A meter cannot certify short dips. Brownout stays enabled throughout.
2. Verify output safe states before/during reset with the resistor defaults.
3. Capture KEY pulses during motor ramp and SD/audio work; no long-held KEY due
   to loop blocking. Confirm repeated taps are benign on the actual module.
4. Test tag present at boot, no tag, missing MP3, rapid remove/replace, all repeats,
   button bounce, long hold and wake while the button is still held.
5. Wait beyond the boost auto-off interval after sleep. Record actual rail voltages
   and current, then separately test GPIO4 wake (powered) and KEY wake (off).
6. If a stage faults, record the log and fix that stage; do not disable brownout or
   insert unrelated delays to hide supply collapse.