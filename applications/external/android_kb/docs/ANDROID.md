# Android App

Landscape fullscreen app that can send HID keyboard and mouse events either:

1. **Via Flipper** — phone BLE Serial → Flipper → USB HID → PC  
2. **Direct Bluetooth** — phone acts as a Bluetooth HID keyboard/mouse to the PC (no Flipper)

## Architecture

```
KeyboardActivity (fullscreen landscape)
  ├─ BLE/BT button (top-left) → connect / disconnect
  ├─ Mode switch (top-center) → Keyboard | Touchpad
  ├─ Settings → output mode + Flipper/PC device + layouts
  ├─ JsonKeyboardView ← assets/layouts/*.json
  └─ TouchpadView → relative mouse move / click / scroll
         │
         ▼
  BridgeSession
    ├─ FlipperBleClient  (Flipper mode)
    └─ DirectHidClient   (Direct Bluetooth HID, API 28+)
```

There is **no** Android IME / system keyboard registration. The app is a normal Activity: input goes to the PC, not into other phone apps.

## Screenshots

See `docs/screenshots/` and the **Screenshots** section in the root `README.md` (keyboard, touchpad, Settings for Flipper / Direct Bluetooth / layouts).

## First-time setup

### A) Via Flipper

1. Pair Flipper in system Bluetooth settings (once).
2. Install the APK (`make apk-install`).
3. Open **Flipper KB Bridge** → **Settings**.
4. Output: **Via Flipper**, select Flipper MAC, enable layouts → **Save**.
5. On Flipper, launch **Android KB Bridge** (USB to PC).
6. Tap the **top-left** connection button until green.
7. Type or use Touchpad.

### B) Direct Bluetooth to PC (no Flipper)

1. Android **9+** phone that supports the HID Device profile (Pixel usually works; many OEMs do not).
2. In the app **Settings**: Output → **Direct Bluetooth to PC**, optionally set **Bluetooth keyboard name**, Save (PC address is optional).
3. Tap the **top-left** connection button.
4. The app tries the **saved / previously paired** PC first.
5. If none is known or reconnect fails, status becomes **Waiting for PC to pair…** and Android asks to make the phone discoverable.
6. On the **PC**, open Bluetooth settings and pair / connect to this phone (HID keyboard/combo).
7. After a successful connection the PC MAC is **saved automatically** for next time.
8. Type or use Touchpad — events go straight to the PC over Bluetooth HID.

If pairing never appears: confirm HID Device is supported on the phone, keep the app in foreground, accept the discoverable prompt, and initiate pairing from the PC.

## Modes (top center)

| Mode | Behavior |
|------|----------|
| **Keyboard** | On-screen layouts; swipe space to cycle |
| **Touchpad** | Relative mouse pad |

Touchpad gestures:

| Gesture | Action |
|---------|--------|
| 1-finger drag | Move cursor |
| 1-finger tap | Left click |
| 2-finger vertical drag | Scroll |
| 2-finger tap | Right click |

## BLE button states

| Color | Meaning | Tap action |
|-------|---------|------------|
| Grey | Disconnected | Connect to saved MAC |
| Orange | Connecting / GATT up | Disconnect |
| Green | Ready (can type) | Disconnect |
| Red | Error | Retry connect |

If MAC is not set, Connect opens Settings.

## Screen / orientation

- Locked to **landscape** (does not rotate).
- Immersive fullscreen (status and navigation bars hidden; swipe edge to peek).
- Keep-screen-on while the keyboard activity is open.
- Current layout is always shown in the toolbar as `Layout: …` (keyboard mode only).
- After a space-bar swipe, a centered banner shows the new layout name for ~1.2 s.
- **Keyboard | Touchpad** toggle is centered at the top of the screen.

## Keyboard layouts

Layouts live as separate JSON files under `android/app/src/main/assets/layouts/`.

Catalog: `layouts/catalog.json`.

### Important: labels only — no OS integration

On-screen layouts (EN / RU / UA, macOS vs MX Mini, etc.) are **for typing convenience on the phone**. They only change key **labels** and which HID usages are sent for each button.

They do **not** talk to the target Mac/PC:

- Switching layout in this app does **not** change the host input language / keyboard source.
- The PC still interprets physical HID key codes with **its own** active layout.
- Example: the “macOS UA” screen shows `і` / `ї` / `є`, but the host prints those characters only if Ukrainian (or matching) input is selected on the Mac/PC. With a US layout active, the same keys produce Latin letters.

RU/UA JSON files therefore reuse the same HID codes as EN (physical key positions). Pick the matching host input source yourself when you need those glyphs.

### Bundled layouts

| Id | Title |
|----|-------|
| `macos_en` | macOS EN |
| `macos_ru` | macOS RU |
| `macos_ua` | macOS UA |
| `number` | Number |
| `mx_mini_en` | Logitech MX Keys Mini EN |
| `mx_mini_ru` | Logitech MX Keys Mini RU |
| `mx_mini_ua` | Logitech MX Keys Mini UA |

### Switching

- **Settings** → check which layouts are enabled (order follows catalog / save order).
- On the keyboard: **swipe left/right on the space bar** to cycle enabled layouts.
  The active layout name appears as a centered banner and stays in the toolbar (`Layout: …`).
- A short tap on space still inserts a space.

### Schema

```json
{
  "id": "macos_en",
  "name": "macOS EN",
  "rows": [
    [
      {
        "label": "q",
        "hid": "0x14",
        "mods": "0x00",
        "span": 1,
        "sticky_mod": false,
        "role": null
      }
    ]
  ]
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | yes (or catalog) | Stable layout id |
| `name` | no | Shown in the toolbar |
| `rows` | yes | Array of rows; each row is an array of keys |
| `label` | yes | Text drawn on the key |
| `hid` | yes | USB HID usage (hex string, e.g. `"0x04"` for `a`) |
| `mods` | no | Modifier bitmask (hex). Default `0x00` |
| `span` | no | Relative width (supports halves, e.g. `1.5`). Default `1` |
| `sticky_mod` | no | If `true`, tap toggles sticky mods instead of sending a key |
| `role` | no | `"space"` enables swipe-to-switch on that key |

### Sticky modifiers

Works for Shift, Ctrl, Option/Alt, Cmd/GUI — any key with `"sticky_mod": true`:

```json
{"label": "⇧", "hid": "0x00", "mods": "0x02", "span": 1.5, "sticky_mod": true}
{"label": "⌘", "hid": "0x00", "mods": "0x08", "sticky_mod": true}
```

- Tap latches the modifier (key highlights green).
- Several sticky mods can be combined (XOR toggle per modifier bit).
- Next normal key is sent with those modifiers, then sticky state clears.

### Modifier bitmask

Same as protocol `mods` byte (see `docs/PROTOCOL.md`):

| Bit | Mask | Key |
|-----|------|-----|
| 0 | `0x01` | Left Ctrl |
| 1 | `0x02` | Left Shift |
| 2 | `0x04` | Left Alt / Option |
| 3 | `0x08` | Left GUI / Cmd |

### HID examples

| Key | `hid` |
|-----|-------|
| `a`–`z` | `0x04`–`0x1D` |
| `1`–`0` | `0x1E`–`0x27` |
| Enter | `0x28` |
| Escape | `0x29` |
| Backspace | `0x2A` |
| Tab | `0x2B` |
| Space | `0x2C` |

## Settings storage

SharedPreferences file `akb_prefs`:

| Key | Meaning |
|-----|---------|
| `output_mode` | `FLIPPER` or `DIRECT_BT` |
| `flipper_mac` | Flipper Bluetooth address |
| `host_mac` | PC Bluetooth address (direct mode) |
| `hid_device_name` | Bluetooth name shown to the PC (direct mode, default `Flipper KB Bridge`) |
| `enabled_layouts` | Comma-separated layout ids |
| `current_layout` | Last active layout id |

## Source map

| Path | Role |
|------|------|
| `KeyboardActivity.kt` | Main UI, connection button, layout cycling, mode switch |
| `SettingsActivity.kt` | Output mode + Flipper/PC device + layouts |
| `BridgeSession.kt` | Routes input to Flipper or Direct HID |
| `hid/DirectHidClient.kt` | BluetoothHidDevice keyboard/mouse |
| `hid/HidReportDescriptor.kt` | HID report descriptor |
| `keyboard/KeyboardLayoutLoader.kt` | Catalog + JSON parse |
| `keyboard/JsonKeyboardView.kt` | Draw keys, sticky mods, space swipe |
| `touchpad/TouchpadView.kt` | Relative mouse pad |
| `ble/FlipperBleClient.kt` | GATT client + write queue (Flipper mode) |
| `ble/BridgeProtocol.kt` | Flipper wire frame encode |
| `prefs/AppPreferences.kt` | Mode + device + layout prefs |
| `assets/layouts/` | Layout JSON files + catalog |

## Build notes

- JDK **17** required for Gradle (`make apk` auto-detects JDK 17).
- System `gradle` is not required; use `android/gradlew`.
- Release APK: `android/app/build/outputs/apk/release/FlipperZeroKbd-<version>.apk` (version from `versionName` in `app/build.gradle.kts`)
