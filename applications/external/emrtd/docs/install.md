# Installing

## What you need

A Flipper Zero, and a travel document of your own. Nothing else: the read, the
cryptography and the export all happen on the device.

The application runs on the official firmware, on Unleashed and on Momentum.
The sources are the same for all three - the application needs no change, and
the mbed TLS configuration these firmwares ship is byte for byte identical, so
PACE behaves the same on each. Development and the release channel build track
official firmware 1.4.3, API 87.1.

## Which package

Only the SDK a package was built against differs between the firmwares, and a
package carries the API version with it. The launcher refuses a mismatch, which
is what `App Too Old` on the Flipper's screen means - not a stale application,
just a package built for a different firmware.
[troubleshooting.md](troubleshooting.md) has that message and its mirror image
in full.

Every release carries one file per firmware. Take the one whose name matches
yours.

## From source, with ufbt

```bash
pip install --upgrade ufbt
git clone https://github.com/filipsedivy/emrtd-flipperzero.git
cd emrtd-flipperzero
```

Then deploy the SDK for the firmware on your device:

```bash
# Official firmware
ufbt update --channel=release

# Unleashed
ufbt update --index-url https://up.unleashedflip.com/directory.json

# Momentum
ufbt update --index-url https://up.momentum-fw.dev/firmware/directory.json
```

and build:

```bash
ufbt launch                        # build, upload and start on a connected Flipper
```

`ufbt` alone builds `dist/emrtd.fap`, which can be copied to `/ext/apps/NFC/`
on the SD card instead.

### Keeping more than one SDK

`ufbt update` replaces whichever SDK was deployed before. To keep several, point
`UFBT_HOME` somewhere else for each:

```bash
UFBT_HOME=~/.ufbt-unleashed ufbt update --index-url https://up.unleashedflip.com/directory.json
UFBT_HOME=~/.ufbt-unleashed ufbt
```

## From the app catalogue

The catalogue entry - the description and the changelog a submission needs -
lives in [.catalog/](../.catalog). Once the application is accepted into the NFC
category it installs from the Flipper mobile application or from
[lab.flipper.net](https://lab.flipper.net/apps), and the catalogue builds it for
every firmware channel itself.

## Next

[usage.md](usage.md) walks through every screen. If the application will not
start or the chip will not answer, [troubleshooting.md](troubleshooting.md) is
organised by what the screen says.
