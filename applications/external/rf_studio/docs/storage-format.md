# Storage Formats

All RF Studio files use the Flipper key-value text format (FlipperFormat). Root folder:
`/ext/apps_data/rf_studio/`.

## Remote: `remotes/<id>.remote`

Schema version 2 (Addendum 001). Header keys may appear in any order and are optional unless
noted. Button records follow `Buttons` in display order, and every record writes all five keys.

```text
Filetype: RF Studio Remote
Version: 2
ID: RDD60247E                     (required; stable, independent of the name)
Name: Home Lighting               (required)
Description: ...                  (written only when set)
Category: Lighting                (General, Lighting, Fans, Wireless Relays, Smart Plugs,
                                   Home Automation, Access Devices, Other)
Favorite: false
Created: 1795097000               (Unix time)
Updated: 1795097155
Last_used: 1795097121
Preferred_radio: Auto             (Auto, Internal, External)
Preferred_frequency: 433920000    (Hz; 0 = none yet. Suggested for the next capture)
Preferred_preset: AM650           (written only when set)
Display_mode: list
Buttons: 2
# Buttons in display order
Button_id: BD3B758B0
Button_name: Power On
Button_signal: SF1DE2CE0          ("none" when unassigned)
Button_created: 1795097155
Button_updated: 1795097155
Button_id: B12AB34CD
...
```

Version handling: files with a version above 2 are refused ("Made by a newer RF Studio"). Older
versions load with defaults for missing keys and are rewritten as version 2 on the next save. If
some buttons cannot be read, the readable ones load and the user is told.

## Signal: `signals/<id>.sub`

Standard Flipper Sub-GHz key or RAW files, unchanged. Imports are copied in; originals are never
modified. Captured RAW files are written by RF Studio in the standard RAW format
(`RAW_Data` lines of up to 512 signed microsecond durations).

## Signal index: `signals/signals.index`

```text
Filetype: RF Studio Signal Index
Version: 1

ID: SF1DE2CE0
Name: 1_on
Frequency: 433920000
Preset: AM650                     (preset name, or Custom)
Protocol: Princeton
Kind: 1                           (0 unknown, 1 fixed, 2 rolling, 3 raw, 4 BinRAW)
Source: 1                         (0 captured, 1 imported)
Verified: false                   (raw signals the user confirmed work)
Created: 1795096889
```

Records are read in order and every record writes every key. Replay eligibility is derived from
Kind, the firmware protocol flags and Verified; it is not stored.

## Settings: `config/settings.conf`

```text
Filetype: RF Studio Settings
Version: 1
Radio_mode: 0                     (0 Auto, 1 Internal, 2 External)
Discover_scope: 1                 (0 common frequencies, 1 all)
Rssi_threshold: -70               (dBm, -95 to -40)
Sort: 0                           (0 name, 1 category, 2 last used)
Default_preset: AM650
Logging: false
```

## Export: `exports/<remote id>/`

```text
<remote name>.remote
signals.index                     (only the signals this remote uses)
signals/<id>.sub
```

Import copies signals that are not already in the library, then saves the remote. If a remote with
the same ID exists, the import gets a new ID and " 2" is added to its name.

## Safe writes

Remotes, the index and settings are written to `<file>.tmp` and swapped into place. On load, if
`<file>` is missing and `<file>.tmp` exists, the swap is finished. If both exist, the `.tmp` file is
an interrupted write and is deleted.
