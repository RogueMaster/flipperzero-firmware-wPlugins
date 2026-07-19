# Build And Install

How to build/install the Android APK, build/launch the Flipper FAP, and flash firmware when APIs diverge.

Related docs:

- `docs/ANDROID.md` — using and customizing the Android app
- `docs/FLIPPER.md` — FAP runtime behavior
- `docs/PROTOCOL.md` — wire format

## Paths

Assumes this repo and a local `flipperzero-firmware` checkout as siblings (override with `FIRMWARE_DIR`):

| Role | Default |
|------|---------|
| This project | `.` (repo root) |
| Firmware tree | `../flipperzero-firmware` |
| Android app | `android/` |
| FAP sources (C, default) | `flipper/android_keyboard_bridge/` |
| FAP sources (Rust port) | `flipper/android_keyboard_bridge_rust/` |

## Makefile shortcuts

From the project root:

```bash
make help
make apk
make apk-install
make apk-release
make apk-release-install
make flipper-link
make flipper-build
make flipper-flash
make flipper-launch
make flipper-cli
```

Override firmware path:

```bash
make flipper-launch FIRMWARE_DIR=/path/to/flipperzero-firmware
```

`make apk` / `make apk-release` auto-detect JDK 17 (`/usr/libexec/java_home`, Homebrew, or common Linux paths). Override if needed:

```bash
make apk-release JAVA_HOME=/path/to/jdk-17
```

## 1. Build the Android APK

Requirements:

- Android SDK (`sdk.dir` in `android/local.properties` — local only, gitignored)
- JDK **17** for CLI builds
- Gradle wrapper (`android/gradlew`) — do **not** rely on a global `gradle` in PATH

### Debug vs release

| Command | Output |
|---------|--------|
| `make apk` | `android/app/build/outputs/apk/debug/FlipperZeroKbd-<version>-debug.apk` |
| `make apk-release` | `android/app/build/outputs/apk/release/FlipperZeroKbd-<version>.apk` |

Release is currently signed with the **debug keystore** so you can sideload without creating a Play Store key.

### Option A: Makefile

```bash
cd /path/to/AndroidKeyboard
make apk                 # debug
make apk-install
make apk-release         # release → FlipperZeroKbd-<version>.apk
make apk-release-install
```

### Option B: Android Studio

1. Open `android/` in this repo
2. Wait for Gradle sync
3. Build → Select Build Variant → `release` (or Run debug as usual)

### Option C: Gradle wrapper

```bash
cd android
./gradlew assembleDebug
./gradlew assembleRelease
```

If `gradlew` is missing:

```bash
# Install JDK 17 + Gradle however you prefer, then:
cd android
gradle wrapper --gradle-version 8.7
```

Install release:

```bash
adb install -r android/app/build/outputs/apk/release/FlipperZeroKbd-*.apk
```

## 2. Use the Android app (no system IME)

The app is a **fullscreen landscape Activity**, not a system keyboard.

1. Pair Flipper in Android Bluetooth settings (once).
2. Open **Flipper KB Bridge**.
3. **Settings** → select paired Flipper, enable layouts → **Save**.
4. Start **Android KB Bridge** on Flipper (USB to PC).
5. Tap the **top-left BLE button** until it is **green**.
6. Type on the on-screen keyboard, or switch to **Touchpad** via the top-center control; swipe the **space bar** to switch layouts (active name is shown in a banner and in the toolbar).

Layouts live in `android/app/src/main/assets/layouts/` (see `docs/ANDROID.md`). Bundled: macOS EN/RU/UA, Number, Logitech MX Keys Mini EN/RU/UA.

App layouts only change on-screen labels / which HID keys are tapped — they do **not** switch the target Mac/PC input language.

## 3. Prepare the Flipper firmware tree

```bash
# From this repo:
make flipper-link
# or manually, from your firmware checkout:
ln -sfn /path/to/AndroidKeyboard/flipper/android_keyboard_bridge \
  applications_user/android_keyboard_bridge
ls -l applications_user/android_keyboard_bridge
```

## 4. Build the Flipper FAP

```bash
cd /path/to/flipperzero-firmware
./fbt build APPSRC=applications_user/android_keyboard_bridge
```

Output example:

```text
build/f7-firmware-D/.extapps/android_keyboard_bridge.fap
```

## 5. Flash Flipper firmware

Flash from the same local tree when you see `ApiTooNew` / `ApiTooOld`, or after updating the firmware branch:

```bash
cd /path/to/flipperzero-firmware
./fbt flash_usb_full
```

Notes: close qFlipper; use a data cable; do not unplug during flash.

## 6. Launch the Flipper app

```bash
cd /path/to/flipperzero-firmware
./fbt launch APPSRC=applications_user/android_keyboard_bridge
```

Or: `make flipper-launch`.

### Optional on-device Flipper screenshots

```bash
AKB_SCREENSHOT=1 make flipper-launch
```

Triple short **Down** saves a PBM under `apps_data/android_keyboard_bridge/` on the SD card. See `docs/FLIPPER.md`.

### Optional Rust FAP

Same BLE protocol; denser comments for reading (`flipperzero-rs` 0.16 / API 87.1):

```bash
rustup target add thumbv7em-none-eabihf
make flipper-rust-build
```

Produces `flipper/android_keyboard_bridge_rust/android_keyboard_bridge_rust.fap` — copy to the Flipper SD Apps folder. Do not run C and Rust FAPs together. See `flipper/android_keyboard_bridge_rust/README.md`.

If the Flipper is not found: reconnect USB, close qFlipper, check CLI:

```bash
python3 scripts/serial_cli.py -p auto
```

## 7. What to do on the Flipper

1. Keep USB connected to the PC.
2. Bluetooth on.
3. Run **Android KB Bridge**.
4. Expect `USB: connected` when the host sees HID.
5. After phone Connect: `Phone: connected`; typing should bump `Frames` and `HID`.

**Back** exits the FAP; **Up** toggles forced backlight (both via input pubsub).

While the FAP owns USB HID, macOS may show the device as Logitech `0x046D` / `0xC529`, not “Flipper”. Serial `/dev/cu.usbmodem…` may disappear — normal. See `docs/FLIPPER.md`.

## 8. Typical workflows

### FAP only

```bash
make flipper-launch
```

### Android only

```bash
make apk-install
```

### Firmware + FAP

```bash
make flipper-flash
make flipper-launch
```

## 9. Common errors

### `ApiTooNew` / `ApiTooOld`

Flash matching firmware: `./fbt flash_usb_full`.

### `Failed to find connected Flipper`

Data cable, USB port, qFlipper closed, not in DFU.

### `gradle: command not found`

Use `./gradlew` or `make apk` (wrapper). Android Studio does not put `gradle` on PATH.

### Java / AGP version errors (e.g. Java 25)

Use JDK 17 (`make apk` or set `JAVA_HOME`).

### Phone shows connected, Flipper `Frames` stuck / only first key

- Update FAP (RPC reclaim + no notify-in-callback deadlock fixes).
- Disconnect/Connect after launching the FAP.
- Confirm `Frames` and `HID` both increase.

### Green BLE on phone, no PC typing

- Flipper shows `USB: connected`.
- Focus a text field on the PC.
- Host sees HID (`hidutil list | grep 0xc529` on macOS).

### BLE button does nothing useful

Set Flipper MAC in Settings first; Flipper must already be paired in system Bluetooth.
