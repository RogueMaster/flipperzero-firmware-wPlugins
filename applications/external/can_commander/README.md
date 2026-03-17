# CAN Commander (Flipper App Guide)

Documentation for the Flipper-side CAN Commander app (`can_commander`).

This document focuses on:
- what each tool is for
- when to use each tool
- practical startup and workflow patterns
- argument meanings you will set most often
- profile formats used by the app (`.injprof`, `.dbcprof`)

Current app version in source: `v2.1.1` (`PROGRAM_VERSION` in `can_commander.h`).

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

## 1. Scope and App Behavior
CAN Commander on Flipper is a UART-driven UI/controller for the CAN Commander ESP32 firmware. It is far more than a basic UI designed to speed up in-field CAN Reverse Engineering and Injection.

Core behavior:
- Flipper sends tool/config actions over UART.
- ESP32 executes CAN operations and streams events back.
- Flipper renders structured dashboards and status/monitor output.
- Flipper also handles profiles/save data/configuration.

Important navigation behavior:
- Pressing `Back` from an active tool stops that tool automatically. (as designed)

## 2. Quick Start and Common Workflows
### Basic startup
1. Connect the CAN Commander to the Flipper via GPIO.
2. Attach CAN wires to the CAN Commander screw terminals **primary bus is right port**
3. Launch the `CAN Commander` app on Flipper (`Apps -> GPIO -> CAN Commander`).
4. Start with a low-risk read workflow, speed test, or unique ID tool to validate CAN traffic.

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
   - `OBD2 Live Data` -> `PID List` for polling live vehicle data (speed, RPM, fuel level, etc)
   - `Fetch VIN` for grabbing vehicle VIN
   - `Fetch DTCs` for pulling error codes (DTCs)
   - `Clear DTCs` to clear codes and reset check engine light (MIL)
3. In DTC display, move through summary/stored/pending/permanent pages and review code lists.

Use this when:
- you are using CAN Commander primarily as a diagnostic/scan workflow

### Workflow 3: Filter to a target ID
1. Go to `Tools` -> `Monitor & Discovery` -> `Filter & Read Frames`.
2. Set `bus`, `mask`, `filter`, `ext_match`, `ext`.
3. Start and view filtered data as it comes through

Use this when:
- read-all is too noisy
- you are tracking a specific ECU/message

### Workflow 4: Write/Inject frames
1. Go to `Tools` -> `Control & Injection` -> `Write Frames`.
2. Set `bus/id/ext/dlc/data/count/interval_ms`.
3. Start and verify transmit counter increases.
4. While in write dashboard, press `OK` to resend current payload using the same count/interval configuration.

Use this when:
- you need deterministic single/burst frame transmission
- you are iterating on a payload quickly

### Workflow 5: Smart Injection setup, run, save, load
1. Go to `Tools` -> `Control & Injection` -> `Smart Injection`.
2. Open a slot and configure name, bus, ID, bytes/bit/field, mux, count, interval.
3. Save the slot and return to Smart Injection.
4. Use `Start` to sync slots and enter live view.
5. Select slot in dashboard and trigger inject with `OK`.
6. Save reusable setups with `Save Profile`; restore with `Load Profile` or via `Profiles`.

Use this when:
- you need a safe way to injection vehicle controls/commands
- this is the **primary** way to inject data onto the bus

It stores live data as it comes in for the selected IDs, tracks the data
then performs in-frame bit/data swaps keeping all unwanted signal changes
untouched. This makes it safe to inject specific signals and commands
without causing unwanted signals to be affected or modified.

### Workflow 6: Load DBC profile and decode
1. Go to `Tools` -> `DBC & Databases` -> `Load DBC Profile`.
2. Choose profile; app loads it, applies it to firmware, and starts decode.
3. Use overview and per-signal pages to inspect decoded values.

Use this when:
- this is the **primary** way to view decoded data from the bus
- you are monitoring a curated set of important signals

You will need to setup injection and decoding profiles for workflows 5 and 6. I made this easy with a webtool. You'll need a DBC with the signals you want to decode or inject (you can also make a DBC in app). Then create those profiles on the website and add them to your flippers SD card. Details in following section.

### DBC conversion note
If you have a standard `.dbc`, you can convert it into CAN Commander profile formats (`.dbcprof` and `.injprof`) at:

<https://cancommander.com>

### For CAN signal reverse-engineering workflow details, jump to [Auto Reverse Engineer](#monitor--discovery).

## 3. Common Arguments (What They Mean)
These are the arguments most users set repeatedly.

| Argument | Meaning | Typical values / notes |
|---|---|---|
| `bus` | Which CAN interface the action targets | `can0` (left port), `can1` (right port), `both` |
| `id` | CAN frame identifier target | Standard (`ext=0`): `0x000..0x7FF`; Extended (`ext=1`): 29-bit |
| `ext` | ID type flag | `0` = standard 11-bit, `1` = extended 29-bit |
| `dlc` | Data length code (payload byte count) | `0..8` bytes (how many bytes are in the frame) |
| `data` | Payload bytes in hex text | Effective byte count depends on `dlc` and parsed bytes |
| `count` | Number of sends/injections in a burst | If omitted/`0`, app normalizes to at least one action |
| `interval_ms` | Delay between repeated sends/injections | Used with `count` for burst timing |

Bus port mapping:
- `can0` is the **right** port on CAN Commander.
- `can1` is the **left** port on CAN Commander.
- `can0` is the **primary** port, most tools default to this.

## 4. Tool Reference (Purpose and When to Use)
### Monitor & Discovery
`Read All Frames`
- Purpose: broad live traffic visibility.
- When to use: initial validation, bus reconnaissance, unknown-network exploration.

`Filter & Read Frames`
- Purpose: constrained read stream by mask/filter rules.
- When to use: isolating one target message family.

`Unique IDs`
- Purpose: Shows # of CAN IDs on the bus and lists them
- When to use: quickly finding active messages on a bus.

`Bit Tracker`
- Purpose: visualize per-bit changes across 8 bytes for a selected stream.
- When to use: identifying which bits coorespond to a specific action
- This is your bread and butter when it comes to reverse engineering CAN signals

`Auto Reverse Engineer`
- Purpose: Allows you to identify which CAN ID(s) contain the action you are attempting to reverse engineer. It consists of 2 phases:
  - **Calibration Phase**: tracks bus changes and adds them to a blacklist to remove background noise. Do **not** perform the target action during this phase.
  - **Monitoring Phase**: shows live changes excluding blacklisted bytes. Perform the target action here to identify matching ID/byte changes.
- When to use: narrowing which signals changed during a controlled action.
- This combined with the Bit Tracker gives you everything you need to to a safe injection to replicate your action via CAN injection!

`Value Tracker`
- Purpose: track byte-by-byte value transitions and change frequency.
- When to use: monitoring state transitions where full decode is not yet known.

`CAN Speed Test`
- Purpose: message-rate tracking.
- When to use: throughput checks and bus-load comparisons.

### Control & Injection
`Write Frames`
- Purpose: direct injection of custom frame payloads (single or burst).
- When to use: active testing, replay, and actuation experiments.

`Smart Injection`
- Purpose: slot-based reusable injection presets with bit/field/mux targeting.
- When to use: after reverse engineering a signal, this is the best way to inject commands as it preserves data you don't want modified while injecting.
- Uses the `.injprof` filetype (or create slots in app)

`Stop Active Tool`
- Purpose: explicit stop command from menu.
- This shouldn't be needed as backing out of a tool sends the stop command but it's here as a backup method.

### Vehicle Diagnostics
`OBD2 Live Data`
- Purpose: live PID polling/streaming.
- When to use: real-time engine/vehicle parameter monitoring.

`Fetch VIN`
- Purpose: one-shot VIN retrieval.
- When to use: when you want to pull the VIN from the ECU.

`Fetch DTCs`
- Purpose: one-shot retrieval of stored/pending/permanent trouble codes.
- When to use: fault triage and diagnosing issues / Check Engine Light

`Clear DTCs`
- Purpose: one-shot diagnostic trouble code clear request.
- When to use: after repairs or controlled test reset.
- Turns off the Check Engine Light

### DBC & Databases
`DBC Decode`
- Purpose: decode live frames into engineering values with signal definitions.
- When to use: once you have data reversed into a dbc file (or `.dbcprof`) this is the **best** way to stream and decode live data. It uses a page system where you get a custom dashboard for each signal as well as an overview showing all signals live.

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
`Connect/Reconnect`
- Purpose: transport re-establish/ping path.

`Bus Config`
- Purpose: per-bus bitrate/mode setup and readback.

`Bus Filters`
- Purpose: per-bus hardware filter setup and clear.

`Stats`, `Ping`, `Get Info`
- Purpose: health/diagnostic checks.

## 5. Dashboard Guide (What You Are Looking At)
`Read All` / `Filtered`
- Frame detail, recent list, and stats views.
- Includes overload indication and pause behavior when traffic is extreme.

`Write`
- Bus, ID, payload bytes, configured count/interval, total sent counter.

`Speed Test`
- Current sampled rate and recent sample history.

`Value Tracker`
- Byte-level current values plus recent value-change history.

`Unique IDs`
- Total discovered IDs and recent discoveries.

`Bit Tracker`
- 8-byte bit grid with changed/frozen indication (`X`) support.

`Auto Reverse Engineer`
- Phase state plus live tracking of changed ID/Bytes

`OBD`
- Live PID values and DTC-specific pages (summary + stored/pending/permanent lists).

`DBC Decode`
- Overview of configured signals plus per-signal pages.
- Uses mapped labels when available.

`Custom Inject`
- Slot status list (name, bus, ID, readiness) and recent event page.

## 6. Status and Error Glossary
`Not connected to CAN Commander`
- UART transport unavailable or ping failure.
- Action: use `Settings` -> `Connect/Reconnect`, verify power/wiring/module state.

`... transport error`
- Command did not complete at transport level.
- Action: reconnect and retry after checking module health.

`TOOL_CONFIG => BAD_ARG`
- Firmware rejected provided arguments.
- Action: recheck key names/values and applicable tool context.

`Inject needs live frame first...`
- Smart Injection slot lacks recent matching source frame.
- Action: ensure matching bus/ID/ext traffic is present before inject.

`Input overload / Rendering paused >1000`
- Read stream too high for safe rendering budget.
- Action: narrow scope using filtered read.

## 7. Performance and Stability Notes
- Read-all has explicit overload guard (~1000 fps threshold for rendering pause).
- Event processing is budgeted to preserve UI responsiveness.
- Dashboard mode minimizes monitor text churn during heavy streams.
- Disconnect during poll flips app to disconnected state and reports status.

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
- Standard `.dbc` files can be converted into CAN Commander profile formats (`.dbcprof` and `.injprof`) at <https://cancommander.com>.

## 9. Known Limits and Safe-Use Notes
- Smart Injection slot count: max `5` per profile (you can create many profiles).
- DBC cached signal count: max `16`.
- DBC mapping entries per signal: max `16` per profile.
- Reverse dashboard tracked changed IDs: max `10`.
- Very high traffic can still require filtering for practical use.
