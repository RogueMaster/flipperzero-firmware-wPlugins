# Screenshots — capture checklist

The Flipper Application Catalog accepts up to **8 PNG screenshots** of the device screen (native resolution **128×64**, 1-bit; the catalog also accepts 2× / 4× upscaled images). This folder is referenced by [`manifest.yml`](../manifest.yml) and by [`README.md`](../README.md).

## How to capture from the device

Two routes work; use whichever you have at hand:

1. **qFlipper** (desktop app): press the *Screenshot* button while the app is on the relevant view. Save the file under the name listed below.
2. **`ufbt cli`** (serial CLI):
   ```
   > screenshot
   ```
   The PNG is saved to the SD card under `/ext/dolphin/screenshots/` (or `/ext/screenshots/` depending on firmware); copy it off the SD card with qFlipper.

After saving, **rename** the file to the slug below and drop it directly in this folder. Keep them at 128×64 (the catalog will upscale).

## Required captures

Each row below is a single screenshot. Follow the **Steps** column on the Flipper, then capture.

| # | File name | View | Steps to reach the state |
|---|---|---|---|
| 1 | `01_main_menu.png` | Main menu | Launch the app from *Apps → GPIO → I2C Tools CLI*. Default selection on `Scan`. |
| 2 | `02_scanner.png` | Scanner | From the main menu, select `Scan` (OK). Run a scan (OK) on a bus with at least 2 devices to show the list. |
| 3 | `03_sender_read.png` | Sender — READ result | Main menu → `Send` (OK). Pick an address (Left/Right), set Reg (Up/Down), set Len = e.g. 8 (Long Right). Press OK to read. Capture the result. |
| 4 | `04_sender_write.png` | Sender — WRITE mode | From the sender, `Long OK` to switch to WRITE. Set Data byte with Long Left/Right. Press OK to send. Capture the "WRITE OK / ERR" indicator. |
| 5 | `05_sender_ascii.png` | Sender — ASCII view | Reproduce capture #3, then `Long Back` to flip to ASCII. (Useful for EEPROMs.) |
| 6 | `06_sniffer.png` | Sniffer | Main menu → `Sniff` (OK). Press OK to start. With a real bus active (e.g. two MCUs talking), capture a frame in the buffer. |
| 7 | `07_infos.png` | Infos page 1 | Main menu → `Infos` (OK). First page shows wiring + 3V3 warning. |
| 8 | `08_infos_keymap.png` | Infos keymap | From `Infos`, press Right to reach the key-map page. |

## Optional / extras (not in `manifest.yml`, useful in `docs/USER_GUIDE.md`)

| File name | Subject |
|---|---|
| `99_cli_scan.png` | Terminal capture (not Flipper screen) of `ufbt cli` showing `i2c scan` output. Save the terminal as PNG. |
| `99_cli_read.png` | Terminal capture of `i2c read 50 00 10 ascii` against an EEPROM. |

## Trim list when submitting

`manifest.yml` lists 6 of the screenshots (1, 2, 3, 4, 6, 7). Add 5 and 8 to the manifest only if all 6 spots are well-covered and you want to use the remaining slots — the catalog accepts up to 8.
