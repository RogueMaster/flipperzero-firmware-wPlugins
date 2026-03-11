# CAN Commander Smart Injection Profile Format (`.injprof`)

This document describes the on-disk format used by the Flipper app to save and load Smart Injection Profiles.

## What a Smart Injection profile is

A Smart Injection profile is a saved preset containing:
- A human-readable set name.
- All 5 Smart Injection slot configurations.
- The currently selected slot index.

This is Flipper-side UI/config persistence. Firmware runtime state is separate.

## File location and naming

Saved Smart Injection profiles are stored in:

`apps_data/can_commander/injection_profiles/`

Each profile is saved as:

`<safe_name>.injprof`

Where `safe_name` is generated from the entered set name:
- letters/digits are kept (letters lowercased)
- spaces, `_`, `-`, `.` become `_`
- repeated separators collapse to one `_`
- trailing `_` is removed
- if empty after sanitization, fallback name is `profile`

Examples:
- `My Horn Set` -> `my_horn_set.injprof`
- `Seat-Heat.v1` -> `seat_heat_v1.injprof`

## Container format

Files use FlipperFormat with this header:

- `Filetype: CANCommanderInjectionProfile`
- `Version: 1`

If header type/version does not match, load fails.

## Top-level keys

- `name` (string): display name for the Smart Injection profile.
- `active_slot` (uint32): selected slot index, `0..4`.

## Per-slot keys

For each slot `N` in `1..5`, the following string keys are stored:

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

Example: for slot 3, keys are `slot3_name`, `slot3_used`, etc.

## Field meanings

- `slotN_name`: UI name shown for the slot (for example `Horn`).
- `slotN_used`: `0/1` flag used by UI logic to indicate configured/active slot profile.
- `slotN_bus`: target bus (`can0`, `can1`, or `both`).
- `slotN_id`: CAN ID hex token used for tracking/injection source frame.
- `slotN_ext`: extended-ID flag (`0` standard 11-bit, `1` extended 29-bit).
- `slotN_mask`: 64-bit mask (hex string) for byte-level masked injection.
- `slotN_value`: 64-bit value (hex string) applied with `mask`.
- `slotN_xor`: 64-bit xor mask (hex string) applied after masked set.
- `slotN_mux`: mux filter enabled (`0/1`).
- `slotN_mux_start`: mux field start bit.
- `slotN_mux_len`: mux field length in bits.
- `slotN_mux_value`: mux field compare value.
- `slotN_sig`: signal/field injection enabled (`0/1`).
- `slotN_sig_start`: signal field start bit.
- `slotN_sig_len`: signal field length in bits.
- `slotN_sig_value`: signal field value.
- `slotN_count`: inject repeat count for this slot preset.
- `slotN_interval_ms`: interval between repeats for this slot preset.

## Defaults and backward compatibility

When loading:
- Missing per-slot keys are filled with defaults.
- Missing or invalid `active_slot` falls back to slot index `0`.
- Defaults are also used when any loaded token is empty.

Default slot values are effectively:
- `name=SlotN`, `used=0`, `bus=can0`, `id=000`, `ext=0`
- `mask=value=xor=0000000000000000`
- `mux=0`, `mux_start=0`, `mux_len=1`, `mux_value=0`
- `sig=0`, `sig_start=0`, `sig_len=1`, `sig_value=0`
- `count=1`, `interval_ms=0`

## Example file

```ini
Filetype: CANCommanderInjectionProfile
Version: 1
name: track_day
slot1_name: Horn
slot1_used: 1
slot1_bus: can0
slot1_id: 1D5
slot1_ext: 0
slot1_mask: 0000000000000000
slot1_value: 0000000000000000
slot1_xor: 0000000000000000
slot1_mux: 0
slot1_mux_start: 0
slot1_mux_len: 1
slot1_mux_value: 0
slot1_sig: 1
slot1_sig_start: 20
slot1_sig_len: 3
slot1_sig_value: 5
slot1_count: 5
slot1_interval_ms: 100
slot2_name: Slot2
slot2_used: 0
slot2_bus: can0
slot2_id: 000
slot2_ext: 0
slot2_mask: 0000000000000000
slot2_value: 0000000000000000
slot2_xor: 0000000000000000
slot2_mux: 0
slot2_mux_start: 0
slot2_mux_len: 1
slot2_mux_value: 0
slot2_sig: 0
slot2_sig_start: 0
slot2_sig_len: 1
slot2_sig_value: 0
slot2_count: 1
slot2_interval_ms: 0
slot3_name: Slot3
slot3_used: 0
slot3_bus: can0
slot3_id: 000
slot3_ext: 0
slot3_mask: 0000000000000000
slot3_value: 0000000000000000
slot3_xor: 0000000000000000
slot3_mux: 0
slot3_mux_start: 0
slot3_mux_len: 1
slot3_mux_value: 0
slot3_sig: 0
slot3_sig_start: 0
slot3_sig_len: 1
slot3_sig_value: 0
slot3_count: 1
slot3_interval_ms: 0
slot4_name: Slot4
slot4_used: 0
slot4_bus: can0
slot4_id: 000
slot4_ext: 0
slot4_mask: 0000000000000000
slot4_value: 0000000000000000
slot4_xor: 0000000000000000
slot4_mux: 0
slot4_mux_start: 0
slot4_mux_len: 1
slot4_mux_value: 0
slot4_sig: 0
slot4_sig_start: 0
slot4_sig_len: 1
slot4_sig_value: 0
slot4_count: 1
slot4_interval_ms: 0
slot5_name: Slot5
slot5_used: 0
slot5_bus: can0
slot5_id: 000
slot5_ext: 0
slot5_mask: 0000000000000000
slot5_value: 0000000000000000
slot5_xor: 0000000000000000
slot5_mux: 0
slot5_mux_start: 0
slot5_mux_len: 1
slot5_mux_value: 0
slot5_sig: 0
slot5_sig_start: 0
slot5_sig_len: 1
slot5_sig_value: 0
slot5_count: 1
slot5_interval_ms: 0
active_slot: 0
```

## Notes

- Backward compatibility:
- Legacy `.cfg` files are still loadable.
- Legacy folder `apps_data/can_commander/slot_sets/` is still scanned.
- Legacy filetype `CANCommanderSlotSet` is still accepted.
- Editing these files manually is supported, but malformed values may be clamped/fallback-filled when loaded.
- The app rewrites normalized slot args on load/save, so formatting/order may change.
