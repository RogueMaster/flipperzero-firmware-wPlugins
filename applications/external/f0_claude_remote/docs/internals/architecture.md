---
description: The three layers of the source, each file's one task, and the files a press passes through.
---

# Architecture

## Layers

| Folder | Layer | Includes from |
|---|---|---|
| `src/core/` | The rules, with no Flipper header, tested on the Mac | Its own folder |
| `src/ui/` | Draws the state it is handed | Its own folder and the core |
| `src/platform/` | What exists only in the firmware | Every folder |

An include points down the layers only, relative to its file, as `"../core/action.h"`.
The core names what it needs, as the keys of `transport.h`,
and the platform supplies it, as `transport_hid.c` does.

## Files

| File | Its one task |
|---|---|
| `src/core/action.h` | Names the remote's actions |
| `src/core/mode.h` | Names the remote's modes |
| `src/core/link.h` | Names the remote's links |
| `src/core/keymap.h`, `src/core/keymap.c` | Maps a button event, a link change or the wait's end to an action, given the mode |
| `src/core/feedback.h`, `src/core/feedback.c` | Says how an action confirms itself |
| `src/core/deed.h`, `src/core/deed.c` | Says which deed an action earns |
| `src/core/display.h`, `src/core/display.c` | Says whether a mode keeps the display lit |
| `src/core/actions.h`, `src/core/actions.c` | Turns an action into calls on the transport |
| `src/core/transport.h` | Names what the core needs of a link |
| `src/ui/view.h`, `src/ui/view.c` | Draws a mode's board |
| `src/ui/view_idle.h`, `src/ui/view_idle.c` | Draws the idle board's buttons |
| `src/platform/main.c` | Hands the firmware's entry point to `app` |
| `src/platform/app.h`, `src/platform/app.c` | Runs the application's event loop |
| `src/platform/backlight.h`, `src/platform/backlight.c` | Holds the display lit while asked to |
| `src/platform/keymap_input.h`, `src/platform/keymap_input.c` | Translates a firmware event for the keymap |
| `src/platform/screen.h`, `src/platform/screen.c` | Keeps what the display shows |
| `src/platform/transport_hid.h`, `src/platform/transport_hid.c` | Routes the core's keys to the link in use |
| `src/platform/transport_link.h` | Names what `transport_hid.c` needs of a link |
| `src/platform/transport_bluetooth.h`, `src/platform/transport_bluetooth.c` | Is the Mac's Bluetooth keyboard for its own life |
| `src/platform/transport_usb.h`, `src/platform/transport_usb.c` | Is the Mac's USB keyboard for its own life |

## A press's path

1. `screen.c`, on the GUI thread: the view port's input callback puts the `InputEvent` on the input queue.
2. `app.c`, on the event loop's thread: takes the event off the input queue.
3. `keymap_input.c`: translates the firmware's key and type into the keymap's own.
4. `keymap.c`: answers the action, given the mode, and moves the mode.
5. `actions.c`: presses and releases the action's keys through `transport.h`.
6. `transport_hid.c`: turns each key into its code, for the link in use.
7. `transport_bluetooth.c` or `transport_usb.c`: puts the code into the link's report.

Before step 5, `app.c` hands `screen.c` the mode and the link `keymap.c` holds,
then `transport_hid.c` that link, which switches to it unless it is the one in use.
After step 7, it hands the firmware's dolphin the deed `deed.c` names,
buzzes as `feedback.c` says, and has `screen.c` flash.

The link takes the other queue:
`transport_bluetooth.c`'s status callback, on the Bt thread,
or `transport_usb.c`'s, in the USB interrupt, puts `true` or `false` on the link queue,
and `app.c` hands it to `keymap.c`,
whose answer `actions.c` sends as a press's would be.

The wait takes a timer in `app.c`, started and stopped as `keymap.c`'s `keymap_wait` answers after every event.
Its end goes to `keymap.c` too, whose answer `actions.c` turns into withdrawing the link,
and `app.c` has `backlight.c` go dark, as `display.c` says of the new mode.

The queues exist because every function of an event loop must be called from the thread that made it
(documented: [`event_loop.h`](https://github.com/flipperdevices/flipperzero-firmware/blob/1.5.1/furi/core/event_loop.h#L15)).
The draw callback, on the GUI thread, reads the mode, the link and the flash as three `_Atomic` words.
