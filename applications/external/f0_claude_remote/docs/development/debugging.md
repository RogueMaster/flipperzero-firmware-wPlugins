---
description: A session on the device, from a powered-off Flipper to a breakpoint, in CLion or in a terminal.
---

# Debugging

The app runs on the device, and GDB reaches it through the Black Magic probe on the WiFi Devboard.
Only our code is stepped through:
the SDK ships no firmware sources, so firmware calls are stepped over.
The board's setup is on [Debug probe](debug-probe.md).

## Before a session

The device must run the firmware the SDK pins,
have Settings → System → Debug on, which otherwise disables its debug port,
and stay unlocked:
a PIN lock drops its USB console, and `mise run launch` fails.
Turn auto-lock off in Settings → Desktop for the session.

Attach the board in this order:

1. Pull the board's cable and power the device off.
2. Seat the board on the GPIO header.
3. Power the device on by holding Back, and enter the PIN.
4. Plug the board's cable, then the device's cable, into the Mac.

If the device will not power on,
take the board off, pull its cable, and hold Back for 30 seconds.

After the session, take the board off and pull its cable, for the [risks](debug-probe.md#risks) it carries.

## In CLion

In a clone [CLion](clion.md) has opened:

1. Pick `Flipper` in the Debug Profiles list on the toolbar.
   CLion lists its own profiles first, so a clone needs this once.
2. Set a breakpoint in our sources.
3. Run the `Debug` configuration.
4. Run the `Launch` configuration.
   CLion stops in disassembly at `flipper_application_alloc_thread`:
   the app is loaded and its symbols are in.
5. Press Resume. CLion stops on the breakpoint.
6. Press Stop when done.
   The red `exit code 137 (interrupted by signal 9:SIGKILL)` is CLion ending GDB after it detached,
   and it is harmless.

## In a terminal

From `.mise/tasks/`:

- `debug` opens GDB on the device in the terminal.

```shell
mise run debug    # GDB on the device; leave it with `quit`
mise run launch   # in another terminal
```

Leave with `quit` rather than killing the process:
this session sets the firmware's debug flag and clears it on the way out,
and with the flag left set the next app to start faults the device.
