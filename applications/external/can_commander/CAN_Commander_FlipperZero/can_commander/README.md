# CAN Commander (Flipper App Guide)

Documentation for the Flipper-side CAN Commander app (`can_commander`).

This document focuses on:
- what each tool is for
- when to use each tool
- practical startup and workflow patterns
- argument meanings you will set most often
- profile formats used by the app (`.injprof`, `.dbcprof`)

Current app version in source: `v2.3.1` (`PROGRAM_VERSION` in `can_commander.h`).

**Minimum supported ESP32 firmware: `v2.2`**. The app handshakes the firmware
version at boot and surfaces a warning if your CAN Commander board is running
older firmware. See [§10](#10-firmware-compatibility-and-versioning) for details.

## Table of Contents
- [1. Scope and App Behavior](#1-scope-and-app-behavior)
- [2. Quick Start and Common Workflows](#2-quick-start-and-common-workflows)
- [3. Common Arguments (What They Mean)](#3-common-arguments-what-they-mean)
- [4. Tool Reference (Purpose and When to Use)](#4-tool-reference-purpose-and-when-to-use)
- [5. Dashboard Guide (What You Are Looking At)](#5-dashboard-guide-what-you-are-looking-at)
- [6. Status and Error Glossary](#6-status-and-error-glossary)
- [7. Performance and Stability Notes](#7-performance-and-stability-notes)
- [8. Profile and Storage Appendix](#8-profile-and-storage-appendix)
- [9. Known Limits and Safe-Use Notes](#9-known-limits-and-safe-use-notes)
- [10. Firmware Compatibility and Versioning](#10-firmware-compatibility-and-versioning)
- [11. WiFi CLI over TCP](#11-wifi-cli-over-tcp)

## 1. Scope and App Behavior
CAN Commander on Flipper is a UART-driven UI/controller for the CAN Commander
ESP32 firmware. It is far more than a basic UI designed to speed up in-field
CAN reverse engineering and injection.

Core behavior:
- Flipper sends tool/config actions over UART.
- ESP32 executes CAN operations and streams events back.
- Flipper renders structured dashboards and status/monitor output.
- Flipper also handles profiles/save data/configuration.

App startup sequence:
1. App enables 5V OTG to power the CAN Commander board (if not already on).
2. Settles for 400 ms while the ESP32 finishes booting.
3. Performs a clean UART connect (`Ping`) and runs the firmware version
   check (`GetInfo`).
4. If the firmware is below `v2.2`, a status screen with the warning and
   update URL is pushed on top of the main menu - back returns to the main
   menu and the warning will not re-show during the session.
5. Otherwise the main menu opens normally.

Connection retries:
- The UART ping is retried up to 3 times with a 50 ms gap before declaring
  the board offline. This eliminates the rare false "Not connected" pop-up
  that previously fired on a single transport glitch.

Important navigation behavior:
- Pressing `Back` from an active tool stops that tool automatically. (as
  designed)

## 2. Quick Start and Common Workflows
### Basic startup
1. Connect the CAN Commander board to the Flipper via GPIO.
2. Attach CAN wires to the CAN Commander screw terminals (**primary bus is
   the right port**).
3. Launch the `CAN Commander` app on Flipper (`Apps -> GPIO -> CAN
   Commander`).
4. If a firmware update warning appears, dismiss it with `Back`. Update at
   <https://www.cancommander.com> when convenient.
5. Start with a low-risk read workflow, speed test, or unique ID tool to
   validate CAN traffic.

### Workflow 1: Read traffic fast
1. Go to `Tools` -> `Monitor & Discovery` -> `Read All Frames`.
2. Confirm frames are visible.
3. If traffic is too heavy, switch to filtered read.

Use this when:
- you want a quick sanity check that bus traffic is present
- you do not yet know which IDs matter

### Workflow 2: Vehicle diagnostics
1. Go to `Tools` -> `Vehicle Diagnostics`.
2. Use one of:
   - `OBD2 Live Data` -> `PID List` for polling live vehicle data (speed,
     RPM, fuel level, etc.)
   - `Fetch VIN` for grabbing the vehicle VIN
   - `Fetch DTCs` for pulling error codes (DTCs)
   - `Clear DTCs` to clear codes and reset the check engine light (MIL)
3. In the DTC display, move through summary/stored/pending/permanent pages
   and review code lists.

Use this when:
- you are using CAN Commander primarily as a diagnostic/scan workflow

### Workflow 3: Filter to a target ID
1. Go to `Tools` -> `Monitor & Discovery` -> `Filter & Read Frames`.
2. Set `bus`, `mask`, `filter`, `ext_match`, `ext`.
3. Start and view filtered data as it comes through.

Use this when:
- read-all is too noisy
- you are tracking a specific ECU/message

### Workflow 4: Write/Inject frames
1. Go to `Tools` -> `Control & Injection` -> `Write Frames`.
2. Set `bus / id / ext / dlc / data / count / interval_ms`.
3. Start and verify the transmit counter increases.
4. While in the write dashboard, press `OK` to resend the current payload
   using the same count/interval configuration.

Use this when:
- you need deterministic single/burst frame transmission
- you are iterating on a payload quickly

### Workflow 5: Smart Injection setup, run, save, load
1. Go to `Tools` -> `Control & Injection` -> `Smart Injection`.
2. Open a slot and configure name, bus, ID, bytes/bit/field, mux, count,
   interval.
3. Save the slot and return to Smart Injection.
4. Use `Start` to sync slots and enter live view.
5. Select a slot in the dashboard and trigger inject with `OK`.
6. Save reusable setups with `Save Profile`; restore with `Load Profile` or
   via `Profiles`.

Use this when:
- you need a safe way to inject vehicle controls/commands
- this is the **primary** way to inject data onto the bus

It stores live data as it comes in for the selected IDs, tracks the data,
then performs in-frame bit/data swaps keeping all unwanted signal changes
untouched. This makes it safe to inject specific signals and commands without
causing unwanted signals to be affected or modified.

### Workflow 6: Replay a captured CAN message
1. Go to `Tools` -> `Control & Injection` -> `Replay`.
2. Set `bus`, `id`, and `ext` for the frame you want to capture, then
   `Start`.
3. On the replay dashboard:
   - Press `OK` to begin **recording**. The screen shows a live elapsed
     timer and frame counter.
   - Trigger the action on the vehicle that produces the target frames.
   - Press `OK` again to stop recording (or wait until the buffer fills).
4. Once frames are captured:
   - Use `Left` / `Right` to set the loop count (1–99).
   - Press `OK` to play back at original timing.
   - Press `Up` to clear the buffer if you want to re-record.
   - Press `OK` or `Down` while replaying to stop.

Use this when:
- you want to capture a sequence (e.g. a button press, a CAN handshake) and
  reproduce it exactly with original inter-frame timing
- a single Write Frames burst is not enough - you need timing fidelity

The Replay tool records up to **384 frames** for one CAN ID. Speed is fixed
at 100% (real-time) playback.

### Workflow 7: Reverse engineer to Bit Tracker / Replay
1. Run `Tools` -> `Monitor & Discovery` -> `Auto Reverse Engineer`.
2. During the **Calibration** phase, hold still - do not perform the target
   action. The tool builds a noise blacklist.
3. During the **Monitoring** phase, perform the target action. Live changes
   excluding blacklisted bytes appear.
4. Highlight a candidate ID with `Up` / `Down`. Then:
   - `Right` → opens **Bit Tracker** with that bus/ID pre-filled (find the
     specific bits responsible).
   - `Left` → opens **Replay** with that bus/ID pre-filled (capture and
     play back the exact bytes).

Use this when:
- you do not yet know which IDs change during a target action
- you want a fast path from "what changes?" to either bit-level analysis or
  replay validation

### Workflow 8: Load DBC profile and decode
1. Go to `Tools` -> `DBC & Databases` -> `Load DBC Profile`.
2. Choose profile; the app loads it, applies it to firmware, and starts
   decode.
3. Use overview and per-signal pages to inspect decoded values.

Use this when:
- this is the **primary** way to view decoded data from the bus
- you are monitoring a curated set of important signals

You will need to set up injection and decoding profiles for workflows 5 and
8. I made this easy with a webtool. You'll need a DBC with the signals you
want to decode or inject (you can also make a DBC in app). Then create those
profiles on the website and add them to your Flipper's SD card. Details in
[§8](#8-profile-and-storage-appendix).

### DBC conversion note
If you have a standard `.dbc`, you can convert it into CAN Commander profile
formats (`.dbcprof` and `.injprof`) at:

<https://cancommander.com>

## 3. Common Arguments (What They Mean)
These are the arguments most users set repeatedly.

| Argument | Meaning | Typical values / notes |
|---|---|---|
| `bus` | Which CAN interface the action targets | `can0` (right port), `can1` (left port), `both` |
| `id` | CAN frame identifier target | Standard (`ext=0`): `0x000..0x7FF`; Extended (`ext=1`): 29-bit |
| `ext` | ID type flag | `0` = standard 11-bit, `1` = extended 29-bit |
| `dlc` | Data length code (payload byte count) | `0..8` bytes (how many bytes are in the frame) |
| `data` | Payload bytes in hex text | Effective byte count depends on `dlc` and parsed bytes |
| `count` | Number of sends/injections in a burst | If omitted/`0`, app normalizes to at least one action |
| `interval_ms` | Delay between repeated sends/injections | Used with `count` for burst timing |

Bus port mapping:
- `can0` is the **right** port on CAN Commander (the **primary** port).
- `can1` is the **left** port on CAN Commander.
- All tools default to `can0` unless explicitly set otherwise.

## 4. Tool Reference (Purpose and When to Use)
### Monitor & Discovery
`Read All Frames`
- Purpose: broad live traffic visibility.
- When to use: initial validation, bus reconnaissance, unknown-network
  exploration.

`Filter & Read Frames`
- Purpose: constrained read stream by mask/filter rules.
- When to use: isolating one target message family.

`Unique IDs`
- Purpose: shows the count of CAN IDs on the bus and lists them.
- When to use: quickly finding active messages on a bus.

`Bit Tracker`
- Purpose: visualize per-bit changes across 8 bytes for a selected stream.
- When to use: identifying which bits correspond to a specific action.
- This is your bread and butter when it comes to reverse engineering CAN
  signals.

`Auto Reverse Engineer`
- Purpose: identify which CAN ID(s) contain the action you are attempting to
  reverse engineer. Two phases:
  - **Calibration Phase**: tracks bus changes and adds them to a blacklist
    to remove background noise. Do **not** perform the target action during
    this phase.
  - **Monitoring Phase**: shows live changes excluding blacklisted bytes.
    Perform the target action here to identify matching ID/byte changes.
- Quick-jump from monitor phase:
  - `Right` on a selected ID → starts **Bit Tracker** with that ID.
  - `Left` on a selected ID → starts **Replay** with that ID.
- When to use: narrowing which signals changed during a controlled action.
- This combined with Bit Tracker / Replay gives you everything you need to
  do a safe injection (or a one-shot replay) to replicate your action.

`Value Tracker`
- Purpose: track byte-by-byte value transitions and change frequency.
- When to use: monitoring state transitions where full decode is not yet
  known.

`CAN Speed Test`
- Purpose: message-rate tracking.
- When to use: throughput checks and bus-load comparisons.

### Control & Injection
`Write Frames`
- Purpose: direct injection of custom frame payloads (single or burst).
- When to use: active testing, replay, and actuation experiments.

`Smart Injection`
- Purpose: slot-based reusable injection presets with bit/field/mux
  targeting.
- When to use: after reverse engineering a signal, this is the best way to
  inject commands as it preserves data you don't want modified while
  injecting.
- Uses the `.injprof` filetype (or create slots in app).

`Replay`
- Purpose: capture a real CAN sequence for one ID at original inter-frame
  timing and play it back N times.
- Set `bus`, `id`, and `ext` in the args editor before starting. `count` and
  speed are **not** in the args editor - count is set on the dashboard with
  Left/Right (1–99); speed is fixed at 100%.
- Dashboard controls (state-aware):
  - **Idle, no frames**: `OK` → start recording.
  - **Recording**: `OK` → stop. Live elapsed-time and frame counter.
  - **Idle, frames captured**:
    - `OK` → play back N loops at 100% speed.
    - `Up` → clear recorded frames.
    - `Left` / `Right` → adjust loop count.
  - **Replaying**: `OK` or `Down` → stop. Live `frame/total` and `loop/total`.
- Capacity: 384 frames per ID. Recording auto-stops when the buffer is
  full.
- When to use: capturing/playing back a captured action when frame timing
  matters and Smart Injection's bit-level approach is overkill.

`Stop Active Tool`
- Purpose: explicit stop command from menu.
- This shouldn't be needed as backing out of a tool sends the stop command,
  but it's here as a backup method.

### Vehicle Diagnostics
`OBD2 Live Data`
- Purpose: live PID polling/streaming.
- When to use: real-time engine/vehicle parameter monitoring.

`Fetch VIN`
- Purpose: one-shot VIN retrieval.
- When to use: when you want to pull the VIN from the ECU.

`Fetch DTCs`
- Purpose: one-shot retrieval of stored/pending/permanent trouble codes.
- When to use: fault triage and diagnosing issues / Check Engine Light.

`Clear DTCs`
- Purpose: one-shot diagnostic trouble code clear request.
- When to use: after repairs or controlled test reset.
- Turns off the Check Engine Light.

### DBC & Databases
`DBC Decode`
- Purpose: decode live frames into engineering values with signal
  definitions.
- When to use: once you have data reversed into a DBC file (or `.dbcprof`),
  this is the **best** way to stream and decode live data. It uses a page
  system where you get a custom dashboard for each signal as well as an
  overview showing all signals live.

`Load DBC Profile`
- Purpose: load/apply stored decode profile and immediately start decode.
- When to use: switching to a known signal profile quickly.

`DBC Database Manager`
- Purpose: manual signal add/remove/list/clear and save profile operations.
- When to use: building or adjusting decode sets directly on Flipper.

### Profiles
`Smart Injection Profiles`
- Purpose: load saved injection profile sets.
- When to use: restoring a known slot package for fast operation.

`DBC Decoding Profiles`
- Purpose: load saved DBC decode profile sets.
- When to use: restoring a curated decode signal set.

### Settings
The Settings screen uses a `VariableItemList`. Most entries are clickable
actions. **LED Brightness** is a left/right slider.

`Connect/Reconnect`
- Purpose: transport re-establish. Closes and reopens the UART, then runs
  the firmware version check. This is the only manual way to retrigger the
  version check during a session.

`Bus Config`
- Purpose: per-bus bitrate/mode setup and readback.

`Bus Filters`
- Purpose: per-bus hardware filter setup and clear.

`WiFi Settings`
- Purpose: configure the CAN Commander board's optional Wi-Fi access point
  for the **TCP CLI**.
- Edits SSID, password, autostart, and AP-on flags. Values are persisted to
  NVS on the ESP32.
- Sends `WIFI_GET_CFG` / `WIFI_SET_CFG` to the firmware.
- See [§11](#11-wifi-cli-over-tcp) for how to connect from any external
  device with a terminal.

`LED Brightness`
- Purpose: set the on-board NeoPixel brightness.
- Use `Left` / `Right` in the Settings list to cycle through `1..10` (10%
  steps). Each change is sent to the firmware immediately.
- Default is `5` (50%). Setting persists until the app exits.

`Stats`, `Ping`, `Get Info`
- Purpose: health/diagnostic checks.

### About (Main Menu)
- Shows the app version and the detected firmware version (or `unknown` if
  the firmware version handshake failed).
- Includes the maintainer name and the update URL.

## 5. Dashboard Guide (What You Are Looking At)
`Read All` / `Filtered`
- Frame detail, recent list, and stats views.
- Includes overload indication and pause behavior when traffic is extreme.

`Write`
- Bus, ID, payload bytes, configured count/interval, total sent counter.
- `OK` resends the configured payload using the same count/interval.

`Speed Test`
- Current sampled rate and recent sample history.

`Value Tracker`
- Byte-level current values plus recent value-change history.

`Unique IDs`
- Total discovered IDs and recent discoveries.

`Bit Tracker`
- 8-byte bit grid with changed/frozen indication (`X`) support.

`Auto Reverse Engineer`
- Phase state plus live tracking of changed ID/Bytes.
- `Right` on a selected ID → starts Bit Tracker. `Left` → starts Replay.

`OBD`
- Live PID values and DTC-specific pages (summary + stored/pending/
  permanent lists).

`DBC Decode`
- Overview of configured signals plus per-signal pages.
- Uses mapped labels when available.

`Custom Inject`
- Slot status list (name, bus, ID, readiness) and recent event page.

`Replay`
- Fixed-row layout (bus/ID/frames stay anchored across states):
  - **Idle (no frames)**: prompts `Press [OK] to Record`.
  - **Idle (with frames)**: shows `Loops: < N >`, control hints
    `[OK] Play  [UP] Clear`, `[L/R] Loops`.
  - **Recording**: `** RECORDING **` with elapsed seconds and live frame
    count; `[OK] Stop`.
  - **Replaying**: `>> REPLAYING`, `Frame: x/y`, `Loop: x/y`,
    `[OK] or [DOWN] to Stop`.
- The dashboard refreshes periodically while the Replay tool is active so
  the timer and frame counters update without keypresses.

## 6. Status and Error Glossary
`Not connected to CAN Commander`
- UART transport unavailable or ping failure (after 3 retries).
- Action: use `Settings` -> `Connect/Reconnect`, verify power/wiring/module
  state.

`Firmware out of date!`
- Boot version check found firmware below `v2.2`, or the board did not
  respond to `GET_INFO` (which means firmware is older than `v2.2`).
- Action: update at <https://www.cancommander.com>. The app keeps working
  but newer features (Replay, LED brightness) require new firmware.

`... transport error`
- Command did not complete at transport level.
- Action: reconnect and retry after checking module health.

`TOOL_CONFIG => BAD_ARG`
- Firmware rejected provided arguments.
- Action: recheck key names/values and applicable tool context.

`Inject needs live frame first...`
- Smart Injection slot lacks a recent matching source frame.
- Action: ensure matching bus/ID/ext traffic is present before inject.

`Input overload / Rendering paused >1000`
- Read stream too high for safe rendering budget.
- Action: narrow scope using filtered read.

## 7. Performance and Stability Notes
- Read-all has explicit overload guard (~1000 fps threshold for rendering
  pause).
- Event processing is budgeted to preserve UI responsiveness.
- Dashboard mode minimizes monitor text churn during heavy streams.
- The monitor scene periodically refreshes for the Reverse and Replay
  dashboards so flash animations and timers update without keypresses.
- Disconnect during poll flips the app to disconnected state and reports
  status.
- The UART ping is retried up to 3 times before declaring "Not connected"
  to absorb single-shot transport glitches.

### RAM and screen streaming
Streaming the Flipper screen via qFlipper or the mobile app increases
memory pressure significantly. v2.3.x trims RAM through:

- Mode-specific dashboard fields share memory via a union (only one tool's
  data is resident at a time; ~3.5 KB saved).
- The DBC signal cache is lazy-allocated - no ~6.8 KB cost unless DBC
  features are actively used.
- Smaller monitor text buffer caps (`1500/1000` bytes; ~1 KB saved).

Notes:
- Even with these reductions, using **DBC Decode** while streaming can
  still push some sessions over the budget. Stop streaming if you hit
  out-of-memory crashes during DBC sessions.

## 8. Profile and Storage Appendix
### 8.1 Storage Paths
- Smart Injection runtime cache: `apps_data/can_commander/custom_inject.cfg`
- Smart Injection profiles: `apps_data/can_commander/injection_profiles/*.injprof`
- DBC decode profiles: `apps_data/can_commander/dbc_profiles/*.dbcprof`

Startup behavior:
- The app creates required profile directories at startup if missing.

### 8.2 Smart Injection Profile Format (`.injprof`)
Header:
- `Filetype: CANCommanderInjectionProfile`
- `Version: 1`

Top-level keys:
- `name`
- `active_slot` (`0..4`)

Per-slot keys (`N=1..5`):
- `slotN_name`
- `slotN_used`
- `slotN_bus`
- `slotN_id`
- `slotN_ext`
- `slotN_mask`
- `slotN_value`
- `slotN_xor`
- `slotN_mux`
- `slotN_mux_start`
- `slotN_mux_len`
- `slotN_mux_value`
- `slotN_sig`
- `slotN_sig_start`
- `slotN_sig_len`
- `slotN_sig_value`
- `slotN_count`
- `slotN_interval_ms`

### 8.3 DBC Decoding Profile Format (`.dbcprof`)
Header:
- `Filetype: CANCommanderDbcProfile`
- `Version: 1`

Top-level keys:
- `name`
- `signal_count` (`0..16`)

Per-signal keys (`X=1..signal_count`):
- `signalX_sid`
- `signalX_name`
- `signalX_bus`
- `signalX_id`
- `signalX_ext`
- `signalX_start`
- `signalX_len`
- `signalX_order`
- `signalX_sign`
- `signalX_factor`
- `signalX_offset`
- `signalX_min`
- `signalX_max`
- `signalX_unit`

Optional mapping keys:
- `signalX_map_count` (`0..16`)
- `signalX_mapY_raw`
- `signalX_mapY_label`

DBC conversion note:
- Standard `.dbc` files can be converted into CAN Commander profile formats
  (`.dbcprof` and `.injprof`) at <https://cancommander.com>.

## 9. Known Limits and Safe-Use Notes
- Smart Injection slot count: max `5` per profile (you can create many
  profiles).
- DBC cached signal count: max `16`.
- DBC mapping entries per signal: max `16` per profile.
- Reverse dashboard tracked changed IDs: max `10`.
- Replay capture buffer: `384` frames per recording (recording auto-stops
  when full).
- Replay loop count range: `1..99`. Speed is fixed at 100% (real-time
  playback).
- LED brightness range: `1..10` (`10%` steps). Default `5`.
- Very high traffic can still require filtering for practical use.
- Streaming the Flipper screen while in DBC Decode can exhaust RAM (see
  [§7](#ram-and-screen-streaming)).

## 10. Firmware Compatibility and Versioning

The app and the ESP32 firmware evolve together. Each release of the app
declares a minimum supported firmware version.

### Current support matrix

| Component | Version |
|---|---|
| App (this build) | `v2.3.1` |
| Required firmware | `v2.2` or newer |

### How the version check works
- The app sends the `GET_INFO` (`0x02`) Flipper command after the UART
  ping at boot (and on `Settings -> Connect/Reconnect`).
- Firmware `v2.2+` responds with a `STATUS_EVT` payload:
  ```
  [0] major:u8
  [1] minor:u8
  [2] string_len:u8
  [3..3+string_len] version string (e.g. "2.2")
  ```
- If the response arrives within 1500 ms and reports `>= v2.2`, the app
  proceeds silently.
- If the response is absent (firmware doesn't implement this feature) or
  reports a lower version, the app pushes a status screen with:
  ```
  Firmware out of date!
  Firmware is: v<detected> (or "unknown")
  Firmware needs: v2.2+
  App: v2.3.1
  Update at:
  www.cancommander.com
  ```
- The warning fires **once per session**. Dismissing with `Back` returns
  to the main menu.

### What stops working with old firmware
- **Replay tool** - requires firmware `v2.2`.
- **LED Brightness** - requires firmware `v2.2`.
- Older tools and dashboards are unaffected.

### Where to update
- Firmware downloads / update notes: <https://www.cancommander.com>.
- The version reported in the **About** screen is the detected firmware
  string, so you can confirm a successful firmware update from the app
  itself.

## 11. WiFi CLI over TCP

The CAN Commander board can broadcast its own Wi-Fi access point and
expose the **full CLI** over a raw TCP socket. Once connected, any device
with a terminal becomes a remote control surface - laptop, phone,
single-board computer, or any of the planned dedicated companion devices.
This is the same command set as the USB CLI: read frames, set filters,
start tools, run injection, manage DBC, etc.

### Why use it
- **No second UART required.** Configure once from the Flipper, then drive
  the board from anything that can open a TCP connection.
- **Cross-platform.** Works from `nc` / `netcat`, `telnet`, PuTTY,
  Termius, JuiceSSH, iSH (iOS), screen, minicom - anything that speaks
  raw TCP.
- **Concurrent.** The Flipper UART session and the Wi-Fi CLI client
  coexist. The CAN Commander firmware tracks ownership so commands don't
  collide.

### One-time setup (from the Flipper)
1. Go to `Settings -> WiFi Settings`.
2. Set the SSID (no spaces) and password (8–63 chars).
3. Set `autostart=1` if you want the AP to come up automatically on every
   boot. Otherwise toggle the AP on manually with `ap=1`.
4. Apply. The values are persisted to NVS on the ESP32 - they survive
   power cycles.
5. The board will print its AP IP and CLI port once started, e.g.:
   ```
   WiFi AP ready ssid=<ssid> ip=192.168.4.1 cli_port=7777
   ```

Defaults if you haven't changed them:
- SSID: `CANCommander`
- Password: `change_me_123` (**change this**)
- TCP port: `7777`
- Autostart: off (start manually with `ap=1` from the Flipper or `wifi
  start` over USB CLI)

### Connect from any device
Join the board's Wi-Fi network (the SSID/password you set), then open a
TCP connection to the board's IP on port 7777.

**macOS / Linux / WSL** - `netcat`:
```sh
nc 192.168.4.1 7777
```

**Windows / cross-platform** - `telnet`:
```
telnet 192.168.4.1 7777
```
(`telnet` may need to be enabled in Windows: *Turn Windows features on or
off → Telnet Client*).

**PuTTY**: set Connection Type to *Raw* (or *Telnet*), Host = board IP,
Port = 7777.

**Phones**: any TCP/raw-mode terminal app - e.g. JuiceSSH (Android, "Local
Telnet" connection), Termius (raw-socket plugin), iSH (iOS - `nc <ip>
7777`).

Once connected, you have a live CLI prompt. Type `help` for the command
list. The same command syntax works as on the USB CLI, including
injection shortcuts (`inject slot=1`, `inject list`, `inject cancel`)
once `custom_inject` is active.

Example session:
```
$ nc 192.168.4.1 7777
help
status
bus show
tool start read_all bus=can0 ascii=0
...
tool stop
```

### Companion devices (planned / in-progress)
Because the TCP CLI uses the same text command set as USB, any device
that can speak TCP can become a fully-featured CAN Commander control
surface. Several dedicated companion apps are in active development:

- **CYD (Cheap Yellow Display, ESP32 + 2.8" touchscreen)** - touchscreen
  UI for injection slots, DBC dashboards, and Replay control over
  Wi-Fi.
- **M5Stack Cardputer** - pocket-size physical-keyboard CLI client tuned
  for the CAN Commander command set.
- **Pineapple Pager** - passive monitoring + injection trigger surface
  designed for in-field use. Developer: Gas Station Hot Dog

These will be released as separate firmware images / apps for their
respective hardware. Track <https://www.cancommander.com> for downloads.

### Switching CLI tips
- Pressing `Back` in the Flipper app stops the active tool - the same
  semantics as USB. If a TCP client is also connected, it sees the tool
  stop event.
- Issue `wifi show` (over USB) or check the boot log for the board's IP
  address.
- Issue `wifi stop` over USB (or `ap=0` from the Flipper) to disable the
  AP without changing SSID/password persistence.
