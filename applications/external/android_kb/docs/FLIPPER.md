# Flipper FAP — Android KB Bridge

External app (`android_keyboard_bridge.fap`) that receives BLE Serial frames from the phone and injects USB HID keyboard reports into the host PC.

## Screenshots

Waiting for the phone (USB already connected to the PC). Hint line shows optional `Dn3=shot` when built with `AKB_SCREENSHOT=1`:

![Flipper waiting for phone](screenshots/flipper-waiting.png)

Phone linked over BLE Serial — ready to type:

![Flipper phone connected](screenshots/flipper-connected.png)

(Captured on-device via triple **Down**; files under `docs/screenshots/`.)

## Runtime behavior

On start:

1. Switches Flipper USB mode to standard HID keyboard (`usb_hid`).
2. Starts the BLE **Serial** profile and advertises.
3. Reclaims the Serial RX callback from the system RPC service (otherwise phone writes go to RPC, not this FAP).
4. Shows status UI; **Back** exits and **Up** toggles forced backlight — both via `RECORD_INPUT_EVENTS` pubsub (independent of the main loop / HID delays).

On stop / Back:

1. Panic-releases all USB keys.
2. Restores the default BLE profile.
3. Restores the previous USB configuration when possible.
4. Restores normal backlight auto-off if Up had locked it on.

## On-screen status

| Line | Meaning |
|------|---------|
| Status text | e.g. `Waiting for phone…`, `Phone linked — type now`, `Shot saved` |
| `USB: connected` / `waiting` | Host accepted the HID interface (`furi_hal_hid_is_connected`) |
| `Phone: connected` / `waiting` | BLE GAP connected (or first RX seen) |
| `Frames:N HID:M` | Parsed protocol frames vs HID commands applied on the app thread |
| Hint line | Default: `Up=light  Back=exit`. With screenshots: `Up=light Dn3=shot Back` (`light*` = backlight forced on) |

Useful checks while typing:

- `Frames` should increase (usually +2 per tap: down + up).
- `HID` should increase in step with frames.
- Status should alternate `HID down` / `HID up`.

## USB identity on the host

Flipper’s stock HID stack advertises as **Logitech-like**:

| Field | Value |
|-------|-------|
| Vendor ID | `0x046D` |
| Product ID | `0xC529` |

On macOS it often appears as a nameless “Composite Device” / keyboard — **not** as “Flipper”:

```bash
hidutil list | grep -i '0xc529\|0x46d'
system_profiler SPUSBDataType | grep -A8 '0xc529'
```

While this FAP owns USB HID, the Flipper CDC serial port (`/dev/cu.usbmodem…`) may disappear. That is expected.

## BLE / RPC reclaim

The firmware BT service opens an RPC session whenever the Serial profile connects and steals the Serial event callback. This FAP:

- registers `bt_set_status_changed_callback`
- on `BtStatusConnected`, reclaims RX with `ble_profile_serial_set_event_callback`
- repeats reclaim for a few main-loop ticks after connect

Do **not** call `ble_profile_serial_notify_buffer_is_empty` from inside the Serial RX callback: the service already holds `buff_size_mtx`, and re-entering it deadlocks after the first packet. Notify is deferred to the main loop.

## HID application path

BLE RX only **enqueues** commands. The main loop applies them with a short gap (~25 ms) so the host sees press before release:

```
BLE callback → protocol parse → hid_queue
main loop    → usb_hid_key_down / key_up → USB
```

Key down/up events map to `furi_hal_hid_kb_press` / `release` with `(mods << 8) | keycode`.

Mouse events map to `furi_hal_hid_mouse_move` / `press` / `release` / `scroll`. Keyboard commands keep a short inter-event gap; mouse moves do not.

## Hardware buttons (input pubsub)

Actions use a subscription to `RECORD_INPUT_EVENTS` (hardware buttons after debounce), not the ViewPort input path — so they still work while the main loop is busy applying HID.

| Button | Action |
|--------|--------|
| **Back** | Exit FAP (`exit_requested`). Accepts Short / Long / Press. |
| **Up** | Toggle forced backlight on/off. Short only (avoids double-toggle). |
| **Down ×3** | *(optional)* Save screen to SD as PBM — only if built with `AKB_SCREENSHOT=1`. |

Forced on uses `sequence_display_backlight_enforce_on`; off restores `sequence_display_backlight_enforce_auto`. Leaving the app always restores auto.

## Optional on-device screenshots

Off by default (compiled out via `#ifdef AKB_SCREENSHOT`). Enable when building/launching:

```bash
AKB_SCREENSHOT=1 make flipper-build
AKB_SCREENSHOT=1 make flipper-launch
```

`application.fam` reads the `AKB_SCREENSHOT` environment variable and adds the `AKB_SCREENSHOT` C define (and a slightly larger stack).

### How to capture

1. Run a build with `AKB_SCREENSHOT=1`.
2. Open **Android KB Bridge** on the Flipper.
3. Press **Down** three times quickly (~within 900 ms).
4. Status becomes `Shot saved` (success beep) or `Shot failed`.

### Where files are stored

On the microSD card:

```text
apps_data/android_keyboard_bridge/shot_YYYYMMDD_HHMMSS.pbm
```

Firmware path: `/ext/apps_data/android_keyboard_bridge/…`  
In qFlipper / File Manager: **SD Card → apps_data → android_keyboard_bridge**.

Format is **PBM P4** (128×64 monochrome). Convert on a computer:

```bash
# ImageMagick
magick shot_20260718_080702.pbm shot.png
# sharper for docs (integer scale, no blur)
magick shot_20260718_080702.pbm -filter point -resize 400% shot.png

# or Pillow
python3 -c "from PIL import Image; Image.open('shot.pbm').save('shot.png')"
```

Install ImageMagick on macOS: `brew install imagemagick`.

## Source map

| File | Role |
|------|------|
| `android_keyboard_bridge.c` | App UI, BLE lifecycle, HID queue pump, Back / Up / optional Down |
| `protocol.c` / `protocol.h` | Frame sync + key/mouse parse |
| `usb_hid_bridge.c` / `.h` | USB HID keyboard + mouse |
| `screenshot.c` / `.h` | Optional triple-Down PBM capture (`#ifdef AKB_SCREENSHOT`) |
| `application.fam` | FAP metadata; reads `AKB_SCREENSHOT` env for `cdefines` |

## Rust parallel port (educational)

There is also a **Rust** implementation written **for educational purposes**
(same protocol and behavior, denser comments / safe-first structure):

`flipper/android_keyboard_bridge_rust/`

It uses [flipperzero-rs](https://github.com/flipperzero-rs/flipperzero) (`flipperzero` 0.16 / API 87.1). Comments are denser there on purpose (learning / comparison). The C FAP remains what `make flipper-launch` builds. The optional screenshot feature is **C-only** for now.

```bash
make flipper-rust-build
# → flipper/android_keyboard_bridge_rust/android_keyboard_bridge_rust.fap
```

Do not run C and Rust FAPs at the same time (both claim BLE Serial + USB HID).

## Build / deploy

See `docs/BUILD.md`. Shortcuts from project root:

```bash
make flipper-link
make flipper-build
AKB_SCREENSHOT=1 make flipper-launch   # optional capture build
make flipper-rust-build                # optional educational Rust FAP
```
