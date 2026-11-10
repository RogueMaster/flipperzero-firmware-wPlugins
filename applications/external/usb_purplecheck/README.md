# USB PurpleCheck

**Created by RogerF5.** A standalone Flipper Zero utility for physical USB control assessments. No companion, PowerShell script or installation is needed on the PC under test.

## Profiles

| Profile | Test | Device observation |
| --- | --- | --- |
| Storage read/write | Isolated 1 MiB FAT12 volume | Successful backend block reads/writes and synthetic return integrity |
| Storage read-only | Same volume, SCSI write protected | Block reads; writes are refused by the Flipper itself |
| USB keyboard | Manually submit a session marker in a focused text field | HID configured and marker reports submitted |

Default language is English. Settings also offer Spanish and Ukrainian, using original Cyrillic bitmap glyphs. The test limit is 30, 60 (default) or 120 seconds. Settings are saved on SD with a CRC32. The startup banner, menus and results work on the device.

## Quick start

1. Install `dist/usb_purplecheck.fap` in `/ext/apps/USB/` with qFlipper. Use a build matching your firmware's API.
2. Close qFlipper and other USB/RPC clients. Open **Apps > USB > USB PurpleCheck** on the physical device.
3. Press OK at the banner, select a profile with Up/Down, and press OK. An eight-second preparation countdown lets you connect to the PC being assessed and release qFlipper's USB lock. Back cancels preparation.
4. Watch the timer and observations. Back stops; the configured limit stops automatically. Eject a mounted test volume before stopping.
5. Select an operator conclusion with Left/Right; OK saves. Up/Down on the result toggles observations and synthetic return results.

Right from the main menu opens Settings; Left opens help. In Settings, Up/Down select a row; Left/Right or OK change it; Back saves. Created by RogerF5 and the version appear below the configurable rows. Last result is available in the menu; Left/Right there toggles the integrity summary.

For the keyboard profile, focus an ordinary text editor and press OK again on the Flipper. A one-second delay precedes the marker. It sends no Enter, shell commands or shortcuts. Back cancels. The firmware HID key map expects a compatible QWERTY layout; check the received text yourself.

## Harmless file transfer check

With **Test files** enabled, the volume contains `SEND.TXT` (4,096 bytes of printable synthetic session data), a zero-filled `RETURN.TXT`, `PROBE.TXT` and instructions. Copy SEND.TXT to a temporary location on the PC, then copy that file back over RETURN.TXT. Safely eject, then stop the trial. The Flipper automatically parses the bounded FAT12 chain, compares all 4,096 bytes to this session's expected pattern and records a CRC32. It distinguishes exact match, different content, no return and read error.

The host must initiate file operations. A USB peripheral cannot independently read the PC's files, force a download, or prove a file was persisted on the PC. SEND block activity can include caching, indexing or security scanning. An exact RETURN match establishes only that the session's synthetic pattern exists in RETURN.TXT on the test image. It does not authenticate the host application or prove a particular transfer route. No personal files are requested or automatically collected. The isolated image is recreated for each storage trial and the rest of the SD card is never exposed.

## Interpreting results

Transport observations and operator conclusions are separate. No response is **inconclusive**. USB enumeration, HID submission and block traffic do not by themselves prove a control-policy bypass. The read-only profile deliberately rejects writes locally; that rejection does not prove the PC enforced a policy. Compare results with a known allowed baseline and endpoint enforcement logs for a policy finding.

Sessions are JSONL under `/ext/apps_data/usb_purplecheck/sessions/`. Reports contain a random session marker, profile, local RTC timestamps, elapsed times, I/O counters, synthetic return summary and the explicitly manual conclusion. They contain no hostname, usernames or personal file contents. Evidence is unsigned and the RTC is not synchronized. `last.bin` stores the latest saved result, `settings.bin` preferences, and `probe.img` the current disposable image. All app writes stay in its own data directory.

## Development

Install [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) on the development machine, then run from this directory:

```sh
ufbt update --channel=release
ufbt
python tools/test.py
```

`python tools/build.py --sdk-home PATH` builds using an already installed SDK. It does not flash firmware. `python tools/package.py` creates a distribution with source, docs, tests and the matching FAP. GitHub Actions builds against the official release SDK and runs native logic tests with sanitizers. Native tests do not replace USB hardware acceptance testing.

## Catalog submission

See [the submission checklist](docs/CATALOG.md), [validation evidence](docs/VALIDATION.md) and [catalog description](docs/catalog-description.md). A source PR or passing build is not a listing: Flipper catalog maintainers review and merge the separate catalog manifest. Screenshots must be genuine unedited qFlipper exports.

GPL-3.0. USB transport is derived from Flipper Devices' Mass Storage application; see [third-party notices](THIRD_PARTY_NOTICES.md). Application code and documentation were developed with AI assistance under RogerF5's direction; reviewers must verify pending hardware cases.
