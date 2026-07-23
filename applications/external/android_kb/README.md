# Android KB Bridge (Flipper)

Receive keyboard and mouse events from the companion Android app over **BLE Serial**, and inject them into a PC as **USB HID**.

## What you need

1. Flipper Zero with this FAP installed
2. USB data cable Flipper ↔ PC
3. Android phone with the companion app ([v0.5.4 release](https://github.com/andybeg/AndroidFlipperZeroKBD/releases/tag/v0.5.4))
4. Phone paired with Flipper in system Bluetooth settings

## How to use

1. Plug Flipper into the PC (USB).
2. Launch **Android KB Bridge** on Flipper.
3. Wait until the screen shows **USB: connected**.
4. On the phone: open the app → Settings → **Via Flipper** → select Flipper → Save.
5. Tap **Connect** (top-left). Status on Flipper should become **Phone: connected**.
6. Type on the phone keyboard or use Touchpad.

## Buttons on Flipper

- **Back** — exit the app
- **Up** — toggle forced backlight
- **Down ×3** — save screenshot (only if built with AKB_SCREENSHOT=1)

Unplugging USB does not exit — use **Back**.

## Notes

- While this FAP owns USB HID, the Flipper serial port may disappear on the host — normal.
- The host may see the device as Logitech-like (0x046D / 0xC529), not as “Flipper”.
- Full project docs (Android Direct Bluetooth mode, protocol, builds): [repository README](https://github.com/andybeg/AndroidFlipperZeroKBD).

## Build

From the project root (needs a local flipperzero-firmware checkout):

- make flipper-link
- make flipper-launch

Or with [uFBT](https://pypi.org/project/ufbt/) from this directory: ufbt launch
