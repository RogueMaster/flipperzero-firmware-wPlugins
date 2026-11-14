---
description: Why USB is a hold on No link, why the console goes, and how the app switches, returns and leaves.
---

# USB

Each fact ends with its kind and its source:
documented, read in firmware 1.5.1's sources, or seen on the device.
A firmware bump can break the second kind, so its links pin tag `1.5.1`.

## A hold on `No link`

The stock Remote builds a second application for USB
(firmware 1.5.1's sources: [`application.fam`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/system/hid_app/application.fam#L2-L19)).
This app stays one, so one `.fap` and one Flipper Apps Catalog listing.

The cable cannot choose:
it is in at every launch by `mise run launch`,
where USB would take the console that launch came through,
and the firmware has no query for what the cable ends in
(documented: [`furi_hal_usb.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/furi_hal_include/furi_hal_usb.h)).

USB is offered on `No link` alone, as a Mac in range returns by itself within a second
(seen: [ble-transport, task 4.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-04-ble-transport/tasks.md)).

Right alone has no hold of its own,
and the long press fires 300 ms after the press
(firmware 1.5.1's sources: [`input.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/input/input.c#L13-L14)),
so `keymap.c` counts Right's hold, as every tap and hold but Back's,
only from a press made on that same screen.

## The console

`usb_hid` has one interface
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L156)),
so the console goes while USB lasts,
and `mise run launch` and frames over RPC wait for leaving it.
On USB no `/dev/cu.usbmodemflip_*` exists and `mise run launch` fails,
and leaving or returning brings both back
(seen: [usb-transport, tasks 6.1, 6.4 and 6.5](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).

## The keyboard's name

With no config the keyboard has no name, on IDs 046D:C529
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L16-L17)),
and a config replaces the IDs along with the name
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L398-L402)),
so the app's config carries those IDs and `Claude Remote`.
It is static, as a switch keeps its pointer for a restart
(firmware 1.5.1's sources: [`furi_hal_usb.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb.c#L374)).
The Mac lists `Claude Remote` on those IDs at 12 Mb/s,
and asks to allow the new accessory the first time
(seen: [usb-transport, task 6.1](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md);
documented: [Apple](https://support.apple.com/en-us/102282)).

## The switch, the return and leaving

| Step | Order |
|---|---|
| The switch | Free Bluetooth, empty the link queue, read the USB mode, unlock, switch, set the status callback |
| The return | Clear the callback, restore the mode, empty the link queue, allocate Bluetooth |
| Leaving, on USB | Clear the callback, restore the mode |

The mode's read, unlock, switch and restore are the stock order
(firmware 1.5.1's sources: [`hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/system/hid_app/hid.c#L177-L187)).
Freeing Bluetooth is its [leaving](bluetooth.md#leaving).

The unlock is there for an open RPC session, which locks the switch
(firmware 1.5.1's sources: [`rpc_cli.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/applications/services/rpc/rpc_cli.c#L49)):
a locked switch answers false
(documented: [`furi_hal_usb.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/furi_hal_include/furi_hal_usb.h#L49-L51)),
which would fail the app's `furi_check`.
A hold sent over an open RPC session switched all the same
(seen: [usb-transport, task 6.5](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).

A switch holds its caller 500 ms
(firmware 1.5.1's sources: [`furi_hal_usb.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb.c#L354)),
on top of Bluetooth's 200 ms,
so `app.c` hands the screen its link before the switch, and the board changes at once,
0.053 s after the long press to USB and 0.055 s to Bluetooth
(seen: [usb-transport, tasks 4.2 and 4.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).

Bluetooth starts again within one run:
after the return the Mac's settings list `Claude Remote`, it pairs and the keys go,
and `/int/.bt.keys` keeps its MD5
(seen: [usb-transport, tasks 4.3 and 6.5](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).

## The link queue

Emptying the queue keeps a `Connected` left by one link from reading as the other's.
An event already pending on the emptied queue then calls nothing
(firmware 1.5.1's sources: [`event_loop.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/furi/core/event_loop.c#L107)).

## The status callback

The callback runs in the USB interrupt
(firmware 1.5.1's sources: [`furi_hal_interrupt.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_interrupt.c#L343), [`furi_hal_usb.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb.c#L320)),
where a queue put must take timeout 0
(firmware 1.5.1's sources: [`message_queue.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/furi/core/message_queue.c#L67-L78)).
Setting it reports a link already up
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L279-L289)),
so USB taken with the cable in needs no press.

`Connected` follows bus activity, before the Mac configures the keyboard
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L433)),
so a press in that gap sends nothing.

A release clears the report with no link too
(firmware 1.5.1's sources: [`furi_hal_usb_hid.c`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/targets/f7/furi_hal/furi_hal_usb_hid.c#L303-L311)),
so a drop during voice leaves no Ctrl+Q behind, as over Bluetooth.

## The 60 s, counted afresh

Each hold starts the 60 s again:
`keymap.c` answers the switch with a start, and starting the timer restarts it
(documented: [`event_loop_timer.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/furi/core/event_loop_timer.h#L69-L74)).
The stop came 60.43 s after the hold on USB and 60.49 s after it on Bluetooth
(seen: [usb-transport, tasks 4.2 and 4.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).

On USB nothing is withdrawn at the end,
as a keyboard on a cable does not advertise.
`Reconnect stopped` ignores the report of a cable connected then,
and none follows until the cable moves,
so the press that starts over puts the current state on the link queue itself:
a cable plugged in there left the screen dark, and Up brought the main screen at once
(seen: [usb-transport, task 6.3](https://github.com/petr-korobeinikov/f0-claude-remote/blob/develop/openspec/changes/archive/2026-11-09-usb-transport/tasks.md)).
That press's tap or hold sends nothing there, as it began on another screen.
