# Pet Thermometer

![Screenshot, Where is the Patch? Arm, Chest, Abdomen](https://screenshot.tbspace.de/sfbjekzhilu.png) ![Patch: Chest, Sensor: 37.03C](https://screenshot.tbspace.de/rbvylfezwat.png)

Flipper Zero app that continuously reads two kinds of body temperature sensors:

- **NFC patches**: NXP NTAG 5 link (ISO 15693) with a TI TMP117 temperature sensor on its
  I2C master, as used by the BeUno patches. The original phone app no longer works, this app
  reads the patches directly.
- **134 kHz chips**: ISO 11784/11785 FDX-B animal chips that carry the temperature in the
  extended data block.

## Usage

1. Apps → Tools → Pet Thermometer
2. Pick the sensor: NFC patch (NTAG 5) or 134 kHz chip (FDX-B)
3. Pick where it is worn: Arm, Chest, Abdomen or Custom
4. Hold the sensor to the back of the Flipper: NFC patches near the top (NFC antenna),
   134 kHz chips on the lower half (LF RFID antenna)

The app keeps measuring (NFC about once per second, 134 kHz about every 0.6 s) and shows the
estimated body temperature in large digits. Below it are the raw sensor temperature and the
tag number (NFC: UID, FDX-B: country code and national number). The green LED blinks on
every reading. When the sensor is removed, the last values stay on screen and the title line
shows a note. The sensor type and location are remembered for the next start.

## Celebration animations

When a tag comes into the field and a file named after its ID exists, the app plays that
animation, with the pet's name on top, before showing the temperature again. Measuring
continues in the background, OK or Back skips the animation. It plays again after the tag
was removed (the "removed" note shows) and put back.

Files go to `/ext/apps_data/ntag5_thermometer/pets/<ID>.anim`, where `<ID>` is the tag number
exactly as the app shows it, e.g. `276-098510868983.anim` for an FDX-B chip or
`E0040150A1B2C3D4.anim` for an NFC patch. For a tag without an animation, the app logs the
file name it looked for (`log` in the Flipper CLI: `No animation /data/pets/<ID>.anim`).

`tools/make_anim.py` converts GIFs or single images (scaled to fit 128x64, black and white)
and has a built-in demo, a hopping paw with hearts:

```sh
tools/make_anim.py cat.gif 276-098510868983.anim --name Sarah
tools/make_anim.py --demo 276-098510868983.anim --name Sarah --preview preview.gif
```

Options: `--loops` (how often it plays, default 2), `--frame-ms`, `--invert` for dark
backgrounds, `--dither` for photos, `--threshold`. The file format is documented in
`pet_animation.h`. Copy the file with qFlipper or
`python3 scripts/storage.py send <file> /ext/apps_data/ntag5_thermometer/pets/<file>`.

## NFC patches (NTAG 5 link)

The command sequence is the one of the original app, sent as ISO 15693 custom commands
(manufacturer code 04, NXP):

| Step | Frame | Meaning |
|---|---|---|
| Select | `22 25 <UID>` | Select the tag, the following commands use the select flag |
| Check | `12 C0 04 3D 00` | READ CONFIG: energy harvesting must be on, it powers the sensor |
| Start | `12 D4 04 48 02 01 0C 00` | WRITE I2C to 0x48: TMP117 config = `0x0C00`, one-shot conversion |
| | wait 50 ms | |
| Pointer | `12 D4 04 48 00 00` | WRITE I2C: register pointer to the temperature register |
| Read | `12 D5 04 C8 01` | READ I2C: 2 bytes into the tag SRAM |
| Fetch | `12 D2 04 00 00` | READ SRAM block 0: flags, T_hi, T_lo, ... |

The skin temperature is the signed big-endian register value / 128. `0x8000` means no
conversion happened.

## 134 kHz chips (FDX-B)

The app uses the LF RFID reader of the firmware with only the FDX-B decoder and restarts the
read after every result (the stock app stops after the first one). Like the stock firmware,
it reads the temperature from the low byte of the extended data block: 8 bit value plus an
odd parity bit, `74 °F + value × 0.2 °F`. That is about 0.11 °C per step, so the app shows one
decimal for these chips. Chips without a valid temperature are reported as "no sensor".

## Body temperature estimate

The body temperature estimate is the piecewise linear model the original app used when its
cloud service gave no estimate (`body_temp_estimator.c`). Chest has its own curve, the other
locations use the arm curve. Outside about 31 to 37 °C the skin temperature is shown unchanged.
The same model is used for both sensor types.

Nothing is written to the tag's memory or configuration. The TMP117 configuration register
and the tag SRAM are volatile.

## Build

```sh
ufbt
ufbt launch
```

CI builds the app with uFBT for official firmware (dev and release), Unleashed and Momentum.
