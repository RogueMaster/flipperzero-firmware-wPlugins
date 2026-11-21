# Architecture

RF Studio is a single Flipper application (the repository root) written in C on the Flipper GUI framework
(ViewDispatcher and SceneManager). Modules follow Specification Section 4.

## Modules

| Folder | Module | Responsibility |
|---|---|---|
| `rf_studio.c` | Application shell | Startup sequence, lifecycle, shared helpers used by scenes |
| `storage/` | Storage Manager | Paths, IDs, recoverable writes, settings, backup |
| `diagnostics/` | Diagnostics Manager | Recent-error ring buffer, optional log file |
| `protocol/` | Protocol Registry | Wraps the firmware protocol registry; classifies kind and replay eligibility |
| `radio/rfs_firmware.*` | Firmware adapter | Firmware detection; everything that differs between firmware families |
| `radio/rfs_radio.*` | Capability and Radio Device Manager | Radios, frequencies, presets, resolve and acquire radios |
| `radio/rfs_tx.*` | Transmission Engine | One transmission at a time on its own thread |
| `capture/` | Signal Capture Engine | Frequency discovery, decoding, raw recording |
| `signal/` | Signal Manager | Signal index, `.sub` validation, import, export, delete |
| `remote/` | Remote Manager | Remote model, load and save, listing, duplicate, export and import |
| `ui/views/` | Remote UI | Custom views: remote operation and capture screens |
| `ui/scenes/` | Screens | One file per scene; `rfs_scene_config.h` lists them |

## Rules

- Scenes never touch radio hardware. Radio access goes through `rfs_radio`, `rfs_tx` and
  `rfs_capture`.
- Only one operation owns a radio at a time: `rfs_radio_acquire` fails with "Radio busy" otherwise.
- Remote code has no protocol logic; buttons reference signals by ID.
- The signal module knows nothing about remotes, except `rfs_remote_signal_usage`, which lives in
  the remote module.
- Library files are written with `rfs_storage_atomic_begin` / `rfs_storage_atomic_commit`
  (write `<file>.tmp`, then swap). `rfs_storage_recover` repairs an interrupted swap.
- No application-level frequency lists. Frequencies and presets come from the firmware settings
  file; validity and transmit permission come from the radio driver and firmware.

## Firmware adapter

`radio/rfs_firmware.c` isolates firmware differences:

- `rfs_firmware_detect` reads origin, version and commit from `version_get()`.
- `rfs_firmware_check_tx` reports the firmware's transmit decision. When the SDK ships
  `lib/subghz/devices/tx.h` (Momentum), detected with `__has_include`, it uses
  `subghz_devices_check_tx`, which explains why a frequency is blocked. Otherwise it reports
  hardware validity only, and the firmware still refuses blocked transmissions in
  `subghz_devices_set_tx`.

Other firmware-specific symbols are avoided: BinRAW is recognised by protocol name, and capture
confirmation compares decoded `Key` values rather than using decoder hash functions that only
some firmware exports. The same source builds against official and Momentum SDKs.

## Threads

| Thread | Owner | Work | Talks to the UI by |
|---|---|---|---|
| GUI | ViewDispatcher | Scenes and views | |
| `RfsTx` | `rfs_tx` | Validate, prepare, transmit, clean up | `RfsEventTxDone` custom event |
| `RfsDiscover` | `rfs_capture` | RSSI sweep over firmware frequencies | `RfsEventCaptureFound` / `Error` |
| SubGhzWorker | firmware | Decode level and duration pairs; write raw samples | `RfsEventCaptureDecoded` |

A 100 ms tick event drives the capture screen. `rfs_capture_tick` reads RSSI, tracks the noise
floor, and starts or ends raw recording.

## Capture

1. **Discovery**: sweep the selected frequency list with the AM650 preset, 2 ms per frequency.
   Each frequency keeps its own noise floor, learned over the first three sweeps and then tracked
   slowly. A hit must exceed both the trigger level (Settings) and its floor by 10 dB, then pass
   three confirmation reads.
2. **Listening**: decode with every decodable firmware protocol. In parallel, raw recording starts
   when RSSI clears the trigger and noise margin, and ends after 700 ms of quiet. Recordings under
   64 samples are treated as noise. The maximum length is 49,152 samples.
3. A decode is accepted only when a second frame decodes to the same code (compared by protocol
   and decoder hash). If the transmission ends after a single decoded frame, that decode is used
   but marked unconfirmed on the result screen. The result screen also warns when the code
   matches another button on the same remote.
4. A decoded signal wins over the raw recording. The result screen shows only values from the
   capture.

## Transmission states

`Idle > Validating > Preparing > Transmitting > Completed | Failed | Cancelled`

Validation refuses missing or invalid files, protocols the firmware lacks, rolling codes, and
protocols without an encoder. Raw files are streamed from the SD card by the firmware file encoder.
The radio is always returned to idle and sleep, whatever the outcome.

## Testing on the device

Builds were checked on a Flipper Zero running Momentum `mntm-012`: startup, Diagnostics, `.sub`
import (fixed and raw), creating a remote, the capture screens, discovery with no transmitter
present (no false triggers), adding buttons from the library, a 100-button remote (scrolling,
missing-signal markers, move, delete), export and import, delete confirmation with usage count,
settings and backup. Transmission and live capture of a real remote still need a test device.
