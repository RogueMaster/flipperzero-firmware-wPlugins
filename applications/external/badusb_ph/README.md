# BBIT BadUSB with placeholders

A Flipper Zero BadUSB app that adds **`[placeholder]` token substitution** and
**per-payload saved configs** on top of a full-featured BadUSB (USB + BLE,
configurable USB/BLE identity, NFC-assisted pairing).

It installs as a **separate app** (`appid = badusb_ph`, shown under **Apps → USB**)
and does not touch the built-in BadUSB. The source is **firmware-agnostic**: one tree
builds as an external FAP for both official Flipper firmware and custom firmware
(Unleashed / RogueMaster).

## Features — by origin

The app is three layers. This section spells out where each feature came from.

### From stock Flipper BadUSB (baseline)
- Run DuckyScript (`.txt`) payloads over **USB** or **BLE**
- Keyboard layout selection
- BLE pair / bond / unpair

### Ported from RogueMaster's BadUSB → made to run on stock firmware
These features existed in **RogueMaster's** BadUSB (not in stock). This app carries
them and they were ported so they build and run against **official** firmware too:
- **Configurable USB identity** — custom VID & PID
- **Configurable USB name** — manufacturer & product strings
- **Configurable BLE name** — the advertised device name
- **Configurable BLE MAC** address
- **NFC pairing emulation** — emulates an NFC tag to help a phone pair over BLE

### Added by whitewhidow (original to this app)
- **`[placeholder]` substitution** — write payloads with tokens like `[host]`,
  `[url]`, `[username]`; the app prompts for them at run time and substitutes the
  values before the DuckyScript executes
- **Per-payload saved configs** — store named sets of placeholder values per payload
  and reuse them, or enter values manually each run

## Placeholder syntax

Anywhere in a `.txt` payload, write `[name]` where `name` is letters, digits or
underscores:

```
DELAY 1500
STRING Connecting to [host] on port [port]
ENTER
STRING Logged in as [username]
```

- Each distinct `[name]` is asked for once; the same `[name]` used multiple times
  gets the same value everywhere.
- A token that isn't a valid name (e.g. `[ ]`, `[a b]`) is left untouched.
- Payloads with **no** placeholders run exactly like stock BadUSB (no extra prompts).

`[...]` is not DuckyScript syntax, so existing payloads are unaffected.

## Run flow

1. Open the app, pick a payload from `/ext/badusb`.
2. If it contains placeholders you get a menu:
   - **a saved config** for this payload (if any), or
   - **Manual entry**.
3. Any placeholder not supplied by the chosen config is requested via the on-screen
   keyboard.
4. After manual entry you're offered **Save** — name it to store a config for reuse,
   or **Run** to launch without saving.
5. The script runs with every `[name]` replaced. Press **OK** to start/stop as usual.

## Configs

- Per payload (keyed by the payload's file name without extension).
- Stored on the SD card at:
  `/ext/apps_data/badusb_ph/configs/<payload_name>/<config_name>.cfg`
- Plain `FlipperFormat` files (`name: value` per line) — editable by hand if you like.

## Firmware compatibility

One source builds as an external FAP for both official and custom firmware. Official
firmware sits at the **lowest API minor** in the shared major (e.g. official 1.4.3 =
API 87.1, Unleashed/RM = 87.x), and the FAP loader accepts a binary whose API minor is
**≤** the firmware's. So **a build made against the official SDK is a universal binary
that loads on official _and_ Unleashed / RogueMaster.** (A build made against the
Unleashed SDK loads on Unleashed/RM but not on the lower-minor official.)

## Build & install

Build with `ufbt` (no in-tree firmware build needed):

```bash
ufbt                       # builds dist/badusb_ph.fap against your current ufbt SDK
ufbt launch                # build + upload + run on a connected Flipper
```

For the **universal / catalog** binary, build against the official release SDK in an
isolated ufbt home (leaves your default `~/.ufbt` untouched):

```bash
export UFBT_HOME=/tmp/ufbt-official
ufbt update --channel=release      # official latest (e.g. 1.4.3 / API 87.1)
ufbt                               # -> dist/badusb_ph.fap (API 87.1, loads everywhere in 87.x)
```

Install: copy `dist/badusb_ph.fap` to `/ext/apps/USB/` on the SD card (qFlipper or the
SDK storage helper); it appears under **Apps → USB**.

```bash
python3 ~/.ufbt/current/scripts/storage.py -p auto send \
    dist/badusb_ph.fap /ext/apps/USB/badusb_ph.fap
```

### Keyboard layouts

The app reuses the layouts firmware installs at `/ext/badusb/assets/layouts`. If a
layout file is missing it falls back to the built-in en-US map.

## Sample payload

`sample_payloads/ph_demo.txt` is a benign demo (types text only) using `[host]`,
`[port]`, `[url]` and `[username]`. Copy it to `/ext/badusb/` to try the flow:

```bash
python3 ~/.ufbt/current/scripts/storage.py -p auto send \
    sample_payloads/ph_demo.txt /ext/badusb/ph_demo.txt
```

## Branches

- **`main`** — the app (this branch; default).
- **`vendored-rm-baseline`** — pristine RogueMaster `bad_usb` snapshot the app was
  forked from. `git diff vendored-rm-baseline..main` shows everything added here
  (placeholder system + the portability work).

## How it was made firmware-agnostic

The RogueMaster baseline depended on RM/Unleashed-only firmware extensions. These were
replaced with portable equivalents so the same source builds against official too:
private `bt_i.h` access dropped (`suppress_pin_screen`, inline `pin_code`);
`bt_get_status()` replaced by status cached from the public
`bt_set_status_changed_callback`; `FURI_HAL_BT_ADV_NAME_LENGTH` →
`FURI_HAL_VERSION_DEVICE_NAME_LENGTH`; `GapPairingCount` → `GapPairingPinCodeVerifyYesNo`;
`#ifndef` fallbacks for `HID_VID/PID_DEFAULT` and `HID_MANUF_PRODUCT_NAME_LEN`; the
required-on-RM `view_dispatcher_enable_queue` kept under a deprecation-suppress pragma.
A BLE first-keystroke settle delay was also added to the connect-then-run path.

## License

Derived from RogueMaster firmware's BadUSB (GPLv3); this app inherits **GPLv3**.
