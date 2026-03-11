# CAN Commander DBC Decoding Profile Format (`.dbcprof`)

## Purpose
`.dbcprof` is a CAN Commander custom profile format for storing a curated DBC subset on Flipper.

Each file stores:
- up to 16 signal definitions
- optional value mapping per signal (`raw -> label`) up to 16 entries per signal

This is not a raw `.dbc` file. It is an app-native profile format.

## Storage Location
- Directory: `apps_data/can_commander/dbc_profiles`
- Extension: `.dbcprof`

## File Header
Every file must start with:

```text
Filetype: CANCommanderDbcProfile
Version: 1
```

If filetype/version do not match, load is rejected.

## Top-Level Keys
- `name`: user-facing profile name
- `signal_count`: number of defined signals in this file (0..16)

## Per-Signal Keys
Signals are 1-indexed: `signal1_*`, `signal2_*`, ... `signalN_*`.

Required/expected keys for each signal:
- `signalX_sid`: signal id (`0..65535`, decimal)
- `signalX_name`: user-facing signal name shown in decode UI (text, optional but recommended)
- `signalX_bus`: `can0`, `can1`, or `both`
- `signalX_id`: CAN ID in hex (no `0x` prefix required)
- `signalX_ext`: `0` or `1` (11-bit vs 29-bit ID)
- `signalX_start`: start bit (`0..63`)
- `signalX_len`: bit length (`1..64`)
- `signalX_order`: `intel` or `motorola`
- `signalX_sign`: `u` (unsigned) or `s` (signed)
- `signalX_factor`: float scaling factor
- `signalX_offset`: float offset
- `signalX_min`: float minimum decoded range
- `signalX_max`: float maximum decoded range
- `signalX_unit`: unit text (short string, can be empty)

## Value Mapping Keys
Optional per signal.

- `signalX_map_count`: number of mappings for this signal (`0..16`)
- For each mapping index `Y`:
  - `signalX_mapY_raw`: integer raw value (signed 64-bit supported)
  - `signalX_mapY_label`: display label (text)

Example meaning:
- `raw=0 -> "Off"`
- `raw=1 -> "On"`

## Runtime Behavior
- **Save source**: app local DBC cache (not firmware list export parsing).
- **Load behavior**: replace-all in firmware (`dbc clear`, then add all signals from file).
- **Decode UI mapping**:
  - if `sid/raw` has a map entry, UI shows mapped label as primary
  - raw/numeric value remains shown as secondary context

## Limits and Validation
- Max signals per file: `16`
- Max value maps per signal: `16`
- On malformed fields, missing required signal fields, or invalid header/version:
  - load fails safely
  - no crash

## Backward Compatibility
- Legacy `.dcfg` files are still loadable.
- Legacy folder `apps_data/can_commander/dbc_configs/` is still scanned.
- Legacy filetype `CANCommanderDbcConfig` is still accepted.

## Example 1: Single Boolean Signal (Off/On)
```text
Filetype: CANCommanderDbcProfile
Version: 1
name: Door_Status
signal_count: 1
signal1_sid: 100
signal1_name: Door
signal1_bus: can0
signal1_id: 1F2
signal1_ext: 0
signal1_start: 12
signal1_len: 1
signal1_order: intel
signal1_sign: u
signal1_factor: 1
signal1_offset: 0
signal1_min: 0
signal1_max: 1
signal1_unit:
signal1_map_count: 2
signal1_map1_raw: 0
signal1_map1_label: Off
signal1_map2_raw: 1
signal1_map2_label: On
```

## Example 2: Multi-Signal Config
```text
Filetype: CANCommanderDbcProfile
Version: 1
name: Powertrain_Core
signal_count: 2
signal1_sid: 10
signal1_bus: can0
signal1_id: 7E8
signal1_ext: 0
signal1_start: 24
signal1_len: 16
signal1_order: intel
signal1_sign: u
signal1_factor: 0.25
signal1_offset: 0
signal1_min: 0
signal1_max: 16383.75
signal1_unit: rpm
signal1_map_count: 0
signal2_sid: 11
signal2_name: Gear
signal2_bus: can0
signal2_id: 1D5
signal2_ext: 0
signal2_start: 20
signal2_len: 3
signal2_order: intel
signal2_sign: u
signal2_factor: 1
signal2_offset: 0
signal2_min: 0
signal2_max: 7
signal2_unit:
signal2_map_count: 4
signal2_map1_raw: 0
signal2_map1_label: Park
signal2_map2_raw: 1
signal2_map2_label: Reverse
signal2_map3_raw: 2
signal2_map3_label: Neutral
signal2_map4_raw: 3
signal2_map4_label: Drive
```
