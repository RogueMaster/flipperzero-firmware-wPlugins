# Ghost Tracker

A paranormal-investigation instrument for the Flipper Zero. Every reading comes from
real hardware (Sub-GHz radio, ADC, a Goertzel filter, raw RF audio) — nothing is a
random-number simulation.

Three tools, plus an in-app pin/QR reference:

- **EMF Meter** — ambient RF scanner (built-in, no wiring) or an external Hall sensor
  with an adjustable calibration margin
- **Spirit Box** — real Sub-GHz frequency sweep with the raw demodulated RX static
  played through the speaker, or a real FM broadcast sweep with an optional TEA5767
  module
- **REM Pod** — capacitive proximity detector via a bare copper-wire antenna

Developed and tested primarily on **Unleashed Firmware**, built with `ufbt`. It's also
verified to build cleanly against the **official Flipper firmware release SDK** (no
Unleashed-exclusive APIs are used), so it works on stock firmware too.

## Build

```bash
cd ~/flipper-zero-apps/ghost_tracker
source ~/flipper-zero-apps/.venv/bin/activate
ufbt            # build
ufbt launch     # build + install + run on a connected Flipper
```

The SDK is pinned to Unleashed (not the default official-firmware channel):

```bash
ufbt update --hw-target f7 --url "https://github.com/DarkFlippers/unleashed-firmware/releases/download/<tag>/flipper-z-f7-sdk-<tag>.zip"
```

## How each module actually works

### EMF Meter

Toggle source with **◀ ▶**.

- **Internal** (no wiring): reuses the CC1101 Sub-GHz radio to round-robin 8 fixed
  frequencies across 300.00–348.00MHz and read real RSSI. Deliberately does **not**
  use the NFC chip — its field-detect circuit only reacts to an *active* 13.56MHz
  reader nearby (a phone, another reader), never to ambient fields, so it never
  moves in practice.
- **External** (optional): a linear Hall sensor (SS49E) wired to pin 16. A handful
  of raw ADC samples are averaged once when this mode starts to capture a
  baseline, then averaged again every poll; the reading is the deviation from
  that baseline, so a magnet (a real, slow DC-ish shift) moves it, not just
  high-frequency jitter. An earlier version used a Goertzel 50/60Hz filter aimed
  at mains-hum ripple instead — more "authentic" on paper, but it could hang the
  whole device after a while and didn't react to an actual magnet at all, so it
  was replaced.
- **▲ ▼ calibration margin**: subtracted from the raw reading before it's turned
  into a level/color (shown on screen as `CAL -X`). Real ambient noise can sit
  well above the fixed thresholds below even at rest, pinning the meter at RED
  forever; the margin re-zeroes the color against your own environment without
  capping how high it can still climb if the signal genuinely gets stronger.

```
SS49E bare-IC pinout (3.0-6.5V):         Wiring to the Flipper:
  ___________                              OUT -> Pin 16 (PC0, ADC)
 | o   o   o |   1 = VCC                   VCC -> Pin 9  (3V3)
 |___________|   2 = GND                   GND -> Pin 11 (GND)
   1   2   3     3 = OUT
  VCC GND OUT

If yours is a breakout module (not the bare IC), follow the silkscreen
labels (S/OUT, -/GND, +/VCC) — physical pin order varies by manufacturer.
```

### Spirit Box

Toggle source with **◀ ▶**: a real Sub-GHz static sweep (no wiring), or a real FM
broadcast sweep with an optional TEA5767 module. Dwell time is adjustable with
**▲ ▼** (20-500ms, default 170ms — matches the SB7 Pro's own recommended sweep
rate; real SB7/SB11 units range 50-350ms, so the Flipper can go faster than any of
them). **OK** toggles mute in both modes.

- **Static** (no wiring): `furi_hal_subghz_set_async_mirror_pin()` mirrors the
  CC1101's raw demodulated RX bitstream directly onto the speaker's GPIO pin, in
  hardware, sweeping 300.00–348.00MHz — real RF static, not a synthesized tone.
  Same technique the community "Radio Scanner" app uses. There's no analog volume
  for a raw digital bitstream, only OK = mute (mirror pin connected/disconnected).
  Flashes/lights the LED when RSSI crosses a real signal threshold.
- **FM** (optional TEA5767 module): real I2C tuning across 87.5-108.0MHz, reading
  the chip's own signal-level/stereo registers back. The module's audio comes out
  its **own L/R jack**, straight to a separate speaker — the Flipper never touches
  that analog signal, only tunes it. OK sends the TEA5767's real hardware mute bit
  (and the app sends one final mute automatically when you back out, so it doesn't
  keep playing after you leave). If the module isn't wired/powered, the screen
  shows "NO MODULE" instead of hanging.

```
TEA5767          Flipper
  VCC    ----->  Pin 9  (3V3)
  GND    ----->  Pin 11 (GND)
  SCL    ----->  Pin 16 (PC0)   *shares this pin with the EMF Hall sensor*
  SDA    ----->  Pin 15 (PC1)   *shares this pin with the REM Pod antenna*
```

Pins 15/16 are shared with the EMF Hall sensor / REM Pod antenna (same physical
I2C bus the Flipper exposes on those pins) — wire in only whichever module you're
using at a time, or use a switch to pick between them.

### REM Pod

```
Antenna: bare rigid copper wire, no extra components
  Wire -> Pin 15 (PC1, ADC)
  Suggested length: 15cm (longer = more sensitive)
```

A floating high-impedance ADC node picks up real ambient 50/60Hz electric-field
coupling; a nearby body/object changes that coupling. 24 raw back-to-back ADC
samples are taken per tick (no oversampling — we want the jitter, not an average),
and the peak-to-peak spread is the signal. Same physical principle a real REM Pod
uses (a high-Z antenna node), not an RC charge-time circuit.

- **◀ ▶** sensitivity, **▲ ▼** alert volume
- On alert: LED strobes + continuous tone; the screen text changes but no longer
  flashes/inverts (kept readable on purpose)

### Pin map

In-app reference too: the About screen (one page per module) and a GitHub QR
code, switchable with **◀ ▶**.

| Pin | Signal | Used by |
|---|---|---|
| 9 | 3V3 | EMF external sensor / TEA5767 power |
| 11 | GND | EMF external sensor / TEA5767 ground |
| 15 (PC1) | ADC or I2C SDA | REM Pod antenna, or TEA5767 SDA |
| 16 (PC0) | ADC or I2C SCL | EMF external Hall sensor, or TEA5767 SCL |

Pins 15/16 double up as the Flipper's external I2C bus (`furi_hal_i2c_handle_external`)
*and* as plain ADC inputs — wire in only whichever module (Hall sensor/REM antenna
vs. TEA5767) you're using at a time, or use a switch to pick between them.

## Future: full FM volume control for the TEA5767

The current TEA5767 integration is the minimal tier: the Flipper only sends I2C
tuning/mute commands, and audio comes out the module's own L/R jack straight to a
separate speaker (the real signal chain in the reference project is
`TEA5767 L/R → PT2257/PT2259 (I2C volume) → PAM8406 amp → its own dedicated
speaker` — the Flipper never touches that analog audio). Adding a PT2257 (I2C
volume control) and PAM8406 (amp) to match the reference project would let the
Flipper also control volume and decode RDS station names over I2C. Not
implemented yet.

## Sources

Code and design decisions in this app were grounded against real SDK headers, real
hardware behavior (verified via live device testing and screen capture — see
`tools/capture_screen.py`), and the following external references:

**Firmware / SDK**
- [DarkFlippers/unleashed-firmware](https://github.com/DarkFlippers/unleashed-firmware) — target firmware; SDK pinned to this release channel instead of the default official-firmware one

**Techniques referenced/adapted**
- [RocketGod-git/Flipper-Zero-Radio-Scanner](https://github.com/RocketGod-git/Flipper-Zero-Radio-Scanner) — confirmed the `furi_hal_subghz_set_async_mirror_pin` technique for playing real demodulated RF audio through the speaker, used in Spirit Box
- [hnsvrdmn/flipper-rfid-metal-detector](https://github.com/hnsvrdmn/flipper-rfid-metal-detector) — researched the 125kHz LF-RFID coil as a possible passive EMF sensor; confirmed it only works as an *active* eddy-current metal detector, which ruled it out for the EMF Meter's "internal" mode
- [pchmielewski1/tea5767-pt22xx-fm-radio-with-volume-control](https://github.com/pchmielewski1/tea5767-pt22xx-fm-radio-with-volume-control) — wiring/register-map reference for the implemented TEA5767 integration in Spirit Box
- [evilpete/flipperzero_protobuf_py](https://github.com/evilpete/flipperzero_protobuf_py) — Python RPC client used in `tools/capture_screen.py` to pull real screenshots over USB serial (same protocol qFlipper's "Remote Control" uses) for visually verifying UI changes during development
- [lincolnloop/python-qrcode](https://github.com/lincolnloop/python-qrcode) — generated the GitHub QR code shown on the About screen, converted offline to an XBM bitmap for `canvas_draw_xbm`

**Official Flipper docs / SDK headers**
- [docs.flipper.net/zero/gpio-and-modules](https://docs.flipper.net/zero/gpio-and-modules) and [docs.flipper.net/zero/rfid](https://docs.flipper.net/zero/rfid) — GPIO pinout and LF-RFID reference
- `furi_hal_i2c_config.h` (ufbt SDK headers) — confirmed `furi_hal_i2c_handle_external` is wired to PC0(SCL)/PC1(SDA) at 100kHz, matching the TEA5767 wiring above
- `furi_hal_adc.h` (ufbt SDK headers) — confirmed `furi_hal_adc_convert_to_voltage` already returns mV (an earlier version of this app multiplied it by 1000 again, assuming volts)

**Background research (general articles, not code)**
- SB7/SB11 spirit box sweep-rate ranges (50-350ms) and the SB7 Pro's recommended 170ms default, informed Spirit Box's sweep timing
- SS49E Hall sensor datasheet, for the bare-IC pinout shown in the About screen
