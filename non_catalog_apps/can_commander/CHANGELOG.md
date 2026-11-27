# CAN Commander — Flipper App Changelog

## v2.3.1 (Current)

This release spans two major feature additions (Replay tool and WiFi CLI
over TCP), firmware compatibility checking, LED brightness control,
RAM-usage work, Reverse-mode quick jumps, and a series of defaults/UX
fixes. It supersedes v2.1.1 (previous release).

---

### Compatibility

- **Minimum ESP32 firmware supported: v2.2**. The app performs a firmware
  version handshake at startup (see below). Pairing this app with firmware
  < v2.2 surfaces an in-app warning but otherwise lets the user continue.

---

### New Features

#### Replay tool (end-to-end)
Companion UI for the firmware's new `replay` tool (Tool ID 12). Records CAN
frames for a single ID with original inter-frame timing, then plays them back.

- New menu entry under **Tools → Control & Injection → Replay**.
- Standalone args editor lets the user set `bus`, `id`, and `ext` before
  starting. `count` and `speed` are **not** exposed in the editor — the
  dashboard owns them at runtime.
- Dedicated dashboard view (`AppDashboardReplay`) with a state-aware layout
  and stable row positions (bus/ID never shifts between states).
- Context-sensitive controls:
  - **IDLE (no frames recorded):** OK → start recording
  - **IDLE (has recorded frames):**
    - OK → play back N loops at 100% speed
    - UP → clear recorded frames (press OK afterward to record again)
    - LEFT / RIGHT → decrement / increment loop count (1–99)
  - **RECORDING:** OK → stop, live elapsed-time counter and frame count
  - **REPLAYING:** OK or DOWN → stop, live `frame/total` and `loop/total`
- Event parsing covers every firmware event format: `ready`, `recording
  started`, `recording stopped`, `buffer full`, `replay started`, `replay
  complete`, `state=… frames=…`, `progress=… remaining_loops=…`, `recording
  cleared`.
- **Auto RE quick-jump**: from the Byte Watcher (Auto Reverse Engineer) monitor
  phase, pressing **Left** on a selected CAN ID stops the current tool and
  starts Replay with the matching `bus`/`id`/`ext` (mirrors the existing Right
  action that starts Bit Tracker).
- Monitor scene polls `cmd=status` on each tick while the replay dashboard is
  active, so the frame-counter and loop-progress update live (firmware does
  not stream per-frame progress events automatically).

#### Firmware version check / compatibility warning
- Added UART protocol parsing of the firmware `GET_INFO` (0x02) response's
  `STATUS_EVT` payload: `[major, minor, string_len, version string…]`.
- Runs at **app boot** (after a settling delay and clean UART open) and on
  **Settings → Connect / Reconnect**. Never runs from implicit reconnects
  in the tool-start path, so tools don't re-trigger the handshake.
- If the firmware version is below `v2.2` (or the command is unsupported and
  times out — indicating pre-2.2 firmware), the app pushes a status screen on
  top of the main menu at boot, displaying:
  ```
  Firmware out of date!
  Firmware is: <detected or "unknown">
  Firmware needs: v2.2+
  App: v2.3.1
  Update at:
  www.cancommander.com
  ```
- Dismissing the warning (back button) returns the user to the main menu and
  sets a "shown" flag so the warning doesn't re-fire during the same session.

#### LED brightness control
- Added UART command `CcCmdLedSetCfg = 0x60` and the `cc_client_led_set_brightness()`
  helper (single-byte payload, 1–10 = 10%–100%).
- **Settings menu** converted from `Submenu` to `VariableItemList`, adding a
  new **LED Brightness** item with left/right cycling through values 1–10 and
  live firmware updates on each change. Default is 5 (50%).
- All existing Settings entries (Connect/Reconnect, Bus Config, Bus Filters,
  WiFi Settings, Stats, Ping, Get Info) preserved as clickable action items
  in the same list.

#### WiFi CLI over TCP (major feature, app + firmware)
The CAN Commander board can now broadcast a Wi-Fi access point and expose
the **full CLI** over TCP. Any device on the AP that can open a raw TCP
socket — laptop (`netcat`/`telnet`), phone, ESP32-based dev kit, etc. —
becomes a remote terminal for the board with **zero hardware UART
required**. Same exact command set as the USB CLI (read frames, set
filters, start tools, drive injection, manage DBC, etc.).

App-side support:
- New `scenes/wifi_menu.c` scene and **WiFi Settings** entry in the
  Settings menu, backed by an `args_wifi_cfg[192]` buffer in the App
  struct.
- Edit SSID, password, autostart, and AP-on flags directly from the
  Flipper UI — values are persisted to NVS on the ESP32.
- Uses `cc_client_wifi_get_cfg` / `cc_client_wifi_set_cfg` (new UART
  command IDs `0x50` / `0x51`).

Connect from any device with a terminal:
```
$ nc <board_ip> 7777        # macOS / Linux / WSL
> telnet <board_ip> 7777    # Windows / cross-platform
```
Or via PuTTY, Termius, JuiceSSH (Android), Termius/iSH (iOS), or any other
TCP/raw-mode terminal.

Companion app hooks (planned / in-progress):
- **CYD (Cheap Yellow Display, ESP32 + 2.8" touchscreen)** — touch UI for
  injection slots, DBC dashboards, and Replay control over Wi-Fi.
- **M5Stack Cardputer** — pocket-size keyboard CLI client tuned for the
  CAN Commander command set.
- **Pineapple Pager** — passive monitoring + injection trigger surface
  while in the field.

These third-party clients all reuse the same TCP CLI exposed by the
firmware, so any new command added to the firmware's CLI is immediately
available to all of them with no protocol changes required.

---

### UX and Defaults

- **About screen** now shows both the app and firmware versions:
  ```
  CAN Commander
  App: v2.3.1
  Firmware: v<detected> (or "unknown")
  Made by
  Matthew KuKanich
  www.cancommander.com
  ```
- **Default bus** changed from `both` to `can0` across all the tools that
  previously defaulted to `both`:
  - `read_all`, `speed`, `valtrack`, `unique_ids`, `dbc_decode`
- **Reverse Engineer defaults** tuned:
  - `calibration_ms`: `10000` → `20000`
  - `monitor_ms`: `20000` → `200000`
- **Write Frames** default `interval_ms` dashboard display now syncs cleanly
  with the tool's internal `period_ms`.
- **Dashboard live updates** during Replay recording/replay no longer require
  button input — the monitor scene periodically refreshes for `AppDashboardReverse`
  and the new `AppDashboardReplay` mode.

---

### Reliability

- **Ping retry**: `app_connect` now retries the UART ping up to 3 times with
  a 50 ms delay between attempts. Fixes the intermittent "CAN Commander not
  connected" pop-up when the ESP32 was fine but the first ping glitched.
- **Boot-time handshake**: app startup now adds a 400 ms settle delay after
  enabling OTG power, then uses `app_connect(force_reconnect=true)` to get
  a clean UART open before the firmware version check runs.

---

### Memory / Performance

Screen streaming through qFlipper / mobile app can push a CAN Commander
session to the RAM limit. Three major reductions were made:

1. **Dashboard model union** — all mode-specific arrays (read, speed,
   valtrack, unique_ids, bittrack, reverse, obd, dbc_decode, custom_inject,
   replay) were moved into an anonymous union of anonymous structs. Since
   only one tool is active at a time, the model's mode-specific section now
   costs the size of the largest member (~1.7 KB) instead of the sum
   (~5.5 KB). **~3.5 KB saved.**
2. **Lazy-allocated DBC signal cache** — `App.dbc_config_signals` changed
   from a fixed `AppDbcSignalCache[16]` array (6.8 KB, always resident) to
   a pointer that is `malloc`'d on first DBC use and `free`'d on reset/exit.
   **~6.8 KB saved** whenever DBC features are not in use.
3. **Monitor buffer caps** — `APP_MONITOR_KEEP_MAX` reduced from `2500` to
   `1500` bytes; `APP_MONITOR_KEEP_TAIL` from `1500` to `1000`. **~1 KB saved.**
- **Stack size** reduced from `14 * 1024` → `10 * 1024` in `application.fam`.

Note: with the DBC array lazy-allocated, using DBC features while streaming
can still push RAM over budget on some qFlipper sessions.

---

### Build / Tooling

- `fap_version` added to `application.fam` (`"2.3"`).
- `application.fam` sources list now includes `scenes/wifi_menu.c`.
- `PROGRAM_VERSION` bumped: `v2.1.1` → `v2.3.1`.
- New compile-time constants in `can_commander.h`:
  - `APP_FW_MIN_MAJOR = 2`, `APP_FW_MIN_MINOR = 2`
  - `APP_FW_MIN_STRING "v2.2"`
  - `APP_FW_UPDATE_URL "www.cancommander.com"`

---

## Previous release — v2.1.1

Baseline for this changelog. Feature set: Read All, Filtered, Write, Speed,
ValTrack, UniqueIds, BitTrack, Reverse, OBD PID, DBC Decode, Custom Inject.
No firmware version handshake, no Replay tool, no LED brightness, no WiFi
settings scene, all mode-specific dashboard fields allocated concurrently.
