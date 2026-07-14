# badusb_ph — RogueMaster BadUSB + placeholders (Flipper Zero FAP)

Fork of RogueMaster firmware's BadUSB that adds `[placeholder]` substitution and
per-payload configs. See `README.md` for app behaviour, build, and the full changelog.

## Repo / baseline workflow

RM's BadUSB has **no standalone upstream repo** (it lives inside the
`flipperzero-firmware-wPlugins` tree), so the original is **vendored**, not a git fork:

- **`master`** — verbatim copy of `applications/main/bad_usb/` from RM firmware at a pinned
  commit (firmware-shipped `resources/` tree omitted). **Never commit here.**
- **`RM`** — baseline + the badusb_ph changes. Default branch. All work goes here.

Current baseline: RM `flipperzero-firmware-wPlugins` **`be48837`** (RM0620-0008-0.420.0).

```sh
git diff master..RM    # exactly the fork's changes vs stock RM BadUSB
```

To update from a newer RM release, re-vendor `bad_usb` onto `master` and rebase `RM` — full
steps in `README-RM.md`.

## Build

NOT `ufbt`. This depends on RM's HID layer, so it builds **in-tree against the RogueMaster
firmware** (a FAP from official BadUSB sources HardFaults at HID init on RM). Steps:

```sh
git clone --depth 1 -b <RM-tag> --filter=blob:none \
    https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git rm-fw
cp -r <this-repo> rm-fw/applications_user/badusb_ph
cd rm-fw && ./fbt fap_badusb_ph        # -> build/f7-firmware-C/.extapps/badusb_ph.fap
```

Install to `/ext/apps/USB/` via the SDK `storage.py` helper. Full detail + the keyboard-layout
note are in `README.md`.

## Don't regress

- Builds on the **RM base**, not official/ufbt — uses RM's `BadUsbHidConfig`,
  `ble_profile_hid_ext`, USB unlock. Official-SDK build crashes on RM.
- Placeholder engine is `helpers/placeholder.{c,h}`; substitution is applied per-line in
  `ducky_script.c::ducky_script_execute_next` before parsing. Payloads without `[tokens]`
  must behave exactly like stock BadUSB.
- `appid=badusb_ph` / `EXTERNAL` so it installs alongside (not over) stock BadUSB.
