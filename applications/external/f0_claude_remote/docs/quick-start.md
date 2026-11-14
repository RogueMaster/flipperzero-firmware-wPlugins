---
description: From a Flipper Zero and a Mac to a prompt dictated and sent from the device.
---

# Quick start

## 1. Install

With a Flipper Zero on firmware 1.5.1, unlocked on USB, on its desktop,
and an Apple Silicon Mac with [mise](https://mise.jdx.dev),
in a clone of the repository:

```shell
mise trust
mise install
mise run setup
mise run launch
```

The app opens on the device.
[Installation](guide/installation.md) has the other route and the details.

## 2. Pair

In the Mac's Bluetooth settings, press Connect on `Claude Remote` under Nearby Devices,
accept the request, and press OK in `Verify code` on the device.
[Pairing](guide/pairing.md) covers the rest.

## 3. Bind the keys

Add this block to the `bindings` array of `~/.claude/keybindings.json`:

```json
{
  "context": "Chat",
  "bindings": {
    "ctrl+q": "voice:pushToTalk",
    "ctrl+x ctrl+f": "command:effort high",
    "ctrl+x ctrl+l": "command:clear",
    "ctrl+x ctrl+x": "command:effort max"
  }
}
```

Then run `/voice hold` once in Claude Code.
[Claude Code setup](guide/claude-code-setup.md) says why, and what else dictation needs.

## 4. Try it

With a Claude Code session focused on the Mac:

1. Hold OK, and speak.
2. Release OK.
3. Wait out `Voice: processing…`.
4. Tap OK: the prompt is sent.

[Using the remote](guide/using-the-remote.md) has every other button.
