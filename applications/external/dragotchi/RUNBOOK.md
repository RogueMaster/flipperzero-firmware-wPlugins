# Dragotchi runbook

Everything needed to install Dragotchi on a Flipper Zero from scratch, plus the
optional ESP32-S2 WiFi devboard. Written for Linux; macOS is the same except for
device paths.

---

## 1. Prerequisites

- **Flipper Zero** on firmware **1.4.3** (API **87.1**). Other firmwares need a
  matching SDK and may not accept this `.fap`.
- **Python 3** and `pip`.
- **ufbt** (Flipper micro-build-tool), pinned to the release SDK:
  ```sh
  pip install ufbt
  ufbt update --channel release     # must resolve to API 87.1
  ```
- USB access to the Flipper. On Linux the Flipper appears as a USB CDC serial
  device `0483:5740` (e.g. `/dev/ttyACM0`). Add yourself to the `dialout` group
  if you get permission errors:
  ```sh
  sudo usermod -aG dialout "$USER"   # then log out/in
  ```
- *(Optional, for the devboard)* `esptool` + `pyserial`: `pip install esptool`.

---

## 2. Build the app

From the repo root:

```sh
ufbt                 # -> dist/dragotchi.fap  (target 7, API 87.1)
```

Run the logic tests (optional, no device needed):

```sh
make -C test/host run     # prints "ALL OK"
```

---

## 3. Install on the Flipper

**Easiest:** with the Flipper connected,

```sh
ufbt launch          # build + upload to /ext/apps/Games/ + run
```

**Manual upload** (also how CI/headless deploys work): copy
`dist/dragotchi.fap` to `/ext/apps/Games/dragotchi.fap` on the SD card, using
either qFlipper, the Flipper mobile app, or the CLI storage API over serial.
Then launch from the Flipper: **Apps → Games → Dragotchi**, or over the CLI:

```
loader open /ext/apps/Games/dragotchi.fap
```

The app creates its own save on first run (fresh egg). Saves are **not**
compatible across save-format bumps — a rejected save just starts a fresh egg.

---

## 4. Optional: WiFi devboard ("signal storm")

The game works fully without this. It adds WiFi AP-density sensing to hunting.
Full detail in **[esp32/README.md](esp32/README.md)**; summary:

1. Connect the **ESP32-S2 WiFi devboard** to the PC by its own USB-C.
2. **Download mode:** hold **BOOT**, tap **RST**, release BOOT → enumerates as
   `303a:0002` (e.g. `/dev/ttyACM1`).
3. Flash MicroPython + upload the reporter:
   ```sh
   cd esp32
   ./flash.sh /dev/ttyACM1            # default baud; re-enter download mode & retry if it errors
   ./upload_main.py /dev/ttyACM1      # after it reboots as MicroPython (303a:4001)
   ```
4. Verify it prints `DRAGO wifi=N rssi=X` on the REPL (it streams continuously;
   the USB `print()` is throttled to ~once per rescan, but the UART stream to the
   Flipper is ~5/second).
5. Unplug from USB and **seat the devboard on the Flipper's GPIO header.** The
   Flipper powers it; it reports over pins 13/14 to the game. Give it ~5 s to
   boot and complete its first scan.
6. In Dragotchi: **Hunt.** When the board is heard, the catch opens the animated
   **Signal Storm** radar screen (boosted odds, guaranteed floor, WiFi-themed
   names, exclusive storm eggs). No board → the normal sub-GHz catch banner.

Reverting to Marauder: re-enter download mode and reflash the Marauder image.

---

## 5. Verifying a headless deploy over serial

If you deploy without qFlipper, the Flipper CLI (115200 8N1 on its ACM port) is
enough to drive and check an install:

```
storage stat /ext/apps/Games/dragotchi.fap     # confirm the file is present
loader open /ext/apps/Games/dragotchi.fap       # launch
loader info                                      # -> Application "Dragotchi" is running
loader close                                     # stop it
```

Note: the CLI `input` command does **not** reach a running FAP, so button
presses must be physical. To confirm game-state changes headlessly, read the
save file back over the storage API rather than trying to script inputs.

---

## 6. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `ufbt` builds for the wrong API | `ufbt update --channel release`; confirm the build footer says `API: 87.1`. |
| Permission denied on `/dev/ttyACM0` | Join `dialout` (see §1); check nothing else holds the port (`fuser /dev/ttyACM0`). |
| Flipper "hangs" over serial | Often a stuck host process holding the port. Kill it (`fuser -k /dev/ttyACM0`); a genuinely wedged Flipper needs a LEFT+BACK reboot. |
| Devboard won't appear for flashing | It only shows on USB in **download mode** (hold BOOT + tap RST). Marauder/MicroPython run modes present differently or not at all. |
| `esptool` errors mid-erase at 460800 | The S2's native USB is flaky at fast-flash baud. Use the default baud (the provided `flash.sh` does) and retry; the chip stays in the bootloader between attempts. |
| Forage never opens the Signal Storm screen | Board not seated on GPIO, not powered, or not running `main.py`; or you foraged before it finished booting/scanning (~5 s). Check the reporter on USB first (§4.4); confirm it's on the Flipper's pins 13/14. |
| "Hunt" does nothing / says "Hunt ready in Ns" | Forage is on its 90 s cooldown. The menu shows "Hunt (Ns)" while cooling down; wait for it to read just "Hunt". |
| Pet died unexpectedly overnight | By design the pet sleeps 20:00–08:00 (needs pause). If it still dies, it was Health=0 before sleep, or on-expedition resolution — check Stats. |

---

## 7. Key facts (for maintainers)

- **Two threads:** a GUI thread and a game-logic thread over a `FuriMessageQueue`.
  The logic thread stack is **4 KB** (sub-GHz sensing overflows a 1 KB stack).
- **Sub-GHz sensing** (`src/hunt_hw.c`) uses the `subghz_devices_*` API,
  receive-only. `subghz_devices_begin()` returns `false` from a FAP but the radio
  still works — its return is intentionally ignored.
- **WiFi link** (`src/esp_link.c`) calls `expansion_disable()` before acquiring
  the USART and `expansion_enable()` after — required for FAP serial access.
- **Pure logic is host-tested** (`test/host/`) with a seedable RNG and a clock
  seam. Device-only code sits behind thin seams (`hunt_hw.c`, `esp_link.c`) that
  are stubbed on the host.
