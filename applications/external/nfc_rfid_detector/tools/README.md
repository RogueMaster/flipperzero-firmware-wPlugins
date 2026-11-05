# Crystal calibration from a computer

Every Flipper's crystal oscillator runs a little fast or slow, causing an error in the LF frequency reading. Calibration measures the crystal and saves the result in the app.

To run the calibration, plug the Flipper in to USB and close qFlipper. Before using the Python tool, exit any app that's open on the Flipper itself.

Calibration progress is displayed on the Flipper, and the result is saved if the calibration run passes consistency checks.

## Using a browser

A browser-based calibration tool is available at https://mfcarroll.github.io/rfid-test-tools/flipper/crystal/ (the QR code on the "Computer calibration" screen).

The page connects to the Flipper over Web Serial and measures the crystal against two time servers, Akamai and Cloudflare.

Open NFC/RFID Detector on the Flipper, press Left for Config, then choose Calibrate with a computer. Leave that screen open while using the page.

## Using Python

`flipper_crystal.py` measures the crystal against NTP and saves the calibration in the app.

The script opens the app on the Computer calibration screen. Usage:

    uv run flipper_crystal.py

or

    pipx run flipper_crystal.py

or

    pip install pyserial
    python3 flipper_crystal.py

- `--minutes N`: how long to measure. 30 by default.
- `--no-send`: print the result without saving it on the Flipper.
- `--server HOST`: the NTP server. `time.cloudflare.com` by default.
- `--log FILE`: write every sample to a file.
- `PORT`: e.g. `COM5` or `/dev/ttyACM0`, only needed when more than one Flipper is connected

## Raw CLI command

While the app is running, it adds `rfid_detector_cal` to the CLI. `loader open /ext/apps/RFID/nfc_rfid_detector.fap cal` starts the app on the Computer calibration screen.

| send | reply |
|---|---|
| (start) | `OK lfcal 64000000 <saved>` |
| `r` | `<count> <pll>`: a 32-bit count at 64 MHz from the crystal (it wraps every 67 s), and `pll` is 1 while the Flipper's clock runs from the crystal |
| `t` | `T <temperature>`: the Flipper's temperature (from the battery gauge) in 0.1 °C |
| `p<elapsed s> <total s> [<estimate> <±>]` + CR | none (updates the progress screen) |
| `s<value> [<±>]` + CR | `SET <value>` once the app has saved the value (within 10 s), otherwise `ERR <reason>` |
| any other character (CR and LF on their own are ignored) | `STOPPED` |

Values are in 0.1 ppm. `+` means fast. Pressing `Cancel` on the Flipper also ends the session with `STOPPED`.
