---
description: Each known failure, its cause and its fix.
---

# Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| No `Verify code` on the device while pairing, and closing the app hangs | The app was launched over the open Apps browser, which holds the dialog | Leave the browser with Back, and launch from the desktop |
| Connect in the Mac's Bluetooth settings times out | After a minute with no link the device stops advertising and shows `Reconnect stopped` | Press any button but Back on the device, then Connect again |
| A recording ends about 2.2 s into the hold | Key repeat is off on the Mac | Turn key repeat on: Claude Code tells a held key by its repeats |
| A dictated prompt is sent cut short | OK was tapped during `Voice: processing…`, which sends the interim text and drops the rest | Wait out `Voice: processing…` before tapping OK |
| Holding Left buzzes, but `/clear` shows no change | The session holds no conversation, so there is nothing to clear | None needed |
| Holding OK, Up, Down or Left buzzes, and Claude Code does nothing | The keys are not bound, or another window is focused | Add the block from [Claude Code setup](claude-code-setup.md), and focus the Claude Code session |
| `mise run launch` fails | A PIN locks the device, which drops its USB console | Unlock it; turn auto-lock off in Settings → Desktop while working |
| A press gives no buzz | No link, dictation on, or a press with no action, such as a tap on Left: nothing was sent | Wait for `No link` to clear, or release OK |
| `No link` goes too soon to hold Right for USB | A paired Mac in range reconnects within a second | Forget `Claude Remote` on the Mac, or turn its Bluetooth off |
| `mise run launch` finds no device on USB | USB takes the device's console while it lasts | Hold Back to leave the app, or hold Right on `No link` with the cable out |
| An editor marks the two effort lines in `keybindings.json` | Claude Code's schema has no space in a command's name | None needed: they run all the same |
