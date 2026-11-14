---
description: Why the app runs a Bluetooth keyboard of its own, and how it pairs, returns after a drop and hands the radio back.
---

# Bluetooth

Each fact ends with its kind and its source:
documented, read in firmware 1.5.1's sources, or seen on the device.
A firmware bump can break the second kind, so its links pin tag `1.5.1`.

## A keyboard of its own

The app starts the firmware's HID profile with its own name, address and key file,
so the stock Remote's bond stays as it was.

The name is `Claude Remote`, written over the whole advertised name:
the profile's own field takes a prefix under 8 characters
(documented: [`hid_profile.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/lib/ble_profile/extra_profiles/hid_profile.h#L14)),
and the advertised name holds 18 bytes
(firmware 1.5.1's sources: [`furi_hal_version.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/furi_hal_include/furi_hal_version.h#L20)),
the first of them its AD type
(firmware 1.5.1's sources: [`gap.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/gap.c#L358)),
which leaves 16 characters.
`Claude` with the device's name ran to 21, so the name is `Claude Remote` alone
(seen: [ble-transport, task 3.1](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).

The address is the device's own with `mac_xor` applied
(documented: [`hid_profile.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/lib/ble_profile/extra_profiles/hid_profile.h#L15);
firmware 1.5.1's sources: [`hid_profile.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/lib/ble_profile/extra_profiles/hid_profile.c#L408-L411)).
The app's `0xC1A0` is neither the stock Remote's 0
(firmware 1.5.1's sources: [`hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/system/hid_app/hid.c#L217))
nor BadUSB's `0x0002`
(firmware 1.5.1's sources: [`bad_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/main/bad_usb/helpers/bad_usb_hid.c#L116)).

The bond goes to `/ext/apps_data/f0_claude_remote/.bt_hid.keys`
(documented: [`bt.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/bt/bt_service/bt.h#L74)),
and the stock Remote's key file keeps its MD5
(seen: [ble-transport, task 4.2](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).

The app advertises whatever the device's Bluetooth setting, as the stock Remote does
(firmware 1.5.1's sources: [`hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/system/hid_app/hid.c#L221)),
though the documentation asks to turn Bluetooth on
(documented: [Controllers](https://docs.flipper.net/zero/apps/controllers)).

## Pairing

Pairing starts from the computer's Bluetooth settings
(documented: [Controllers](https://docs.flipper.net/zero/apps/controllers)),
and the firmware draws the `Verify code` dialog, with Cancel and OK, over the app
(firmware 1.5.1's sources: [`bt.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/bt/bt_service/bt.c#L114-L118)),
so the app has no screen for it.
The link reads as up only once pairing completes
(firmware 1.5.1's sources: [`gap.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/gap.c#L280)),
so `Reconnecting...` shows until OK.

The open Apps browser blocks pairing.
The browser is a dialog of the dialogs service
(firmware 1.5.1's sources: [`loader_applications.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/loader/loader_applications.c#L110)),
which shows one dialog at a time
(firmware 1.5.1's sources: [`dialogs.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/dialogs/dialogs.c#L49-L51)),
so `Verify code` waits behind it.
Launched over the browser, the app showed no dialog, the Mac's request failed after 30 s, and closing hung
(seen: [ble-transport, task 4.1](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).
Launch from the desktop.

## After a drop

The firmware advertises again after a disconnect
(firmware 1.5.1's sources: [`gap.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/gap.c#L143)),
so the app holds no reconnect code.
Disconnected from its settings, the Mac reconnected by itself in 866 ms
(seen: [ble-transport, task 4.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).

The app stops advertising 60 s after its opening, a drop or the press that starts over,
and that press starts it again
(documented: [`furi_hal_bt.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/furi_hal_include/furi_hal_bt.h#L132-L138)).
The 60 s are the firmware's own fast phase, every 80–100 ms, before every 1–2.5 s
(firmware 1.5.1's sources: [`gap.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/gap.c#L430-L435)),
where a connect took 28 s and once timed out at 31 s
(seen: [ble-transport, task 4.5](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).
Stopping cuts a connection in progress
(firmware 1.5.1's sources: [`gap.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/gap.c#L481-L483)),
waits until the radio is idle
(firmware 1.5.1's sources: [`furi_hal_bt.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_bt.c#L261-L267))
and reports the link down itself
(firmware 1.5.1's sources: [`bt.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/bt/bt_service/bt.c#L312-L313)),
so `Reconnect stopped` ignores link reports until a press.

A pressed key stays in the profile's report until a release clears it
(firmware 1.5.1's sources: [`hid_profile.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/lib/ble_profile/extra_profiles/hid_profile.c#L188-L227)),
so Ctrl+Q, held for voice, would go out with the next key after the link returns.
A drop during voice therefore releases it at once, with no link:
the report clears whatever the update answers,
since a failed update only logs
(firmware 1.5.1's sources: [`gatt.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/ble_glue/furi_ble/gatt.c#L139-L144)).

## Leaving

The app ends the link as the stock Remote does:
disconnect, 200 ms for the second core to write its storage, the default key path, the default profile
(firmware 1.5.1's sources: [`hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/system/hid_app/hid.c#L230-L239);
documented: [`bt.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/bt/bt_service/bt.h#L46)).
With the setting on, the desktop then shows the Bluetooth mark,
and the device's own key file keeps its MD5
(seen: [ble-transport, task 4.7](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).
