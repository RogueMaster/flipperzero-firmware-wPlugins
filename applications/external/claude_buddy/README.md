# Claude Buddy

Claude Buddy turns your Flipper Zero into a physical remote and status display for Claude — with haptic, audio, and LED feedback for every significant event, and buttons that replace the keyboard for the actions you use most.

## Two modes

Pick whichever one matches how you use Claude.  Switch on-device at any time: long-press **Right → MENU**, then the top row.

| Mode | Pairs with | Transport | What the buttons do |
|---|---|---|---|
| **Claude Code (USB/BLE)** *(default)* | Claude Code in a terminal, via the companion plugin + Python host bridge | USB or BLE (USB preferred when plugged in) | Forward keystrokes: Enter, Esc, voice dictation, Ctrl+C, slash-command menu, arrows, etc. |
| **Claude Desktop (BLE)** | Claude Desktop app, directly over Anthropic's [Hardware Buddy](https://github.com/anthropics/claude-desktop-buddy) protocol (Nordic UART Service) | BLE only | Show live session status + transcript from Claude Desktop; Allow / Deny permission prompts on-device. |

Both modes give you the same LED / sound / vibration feedback for working, success, error, and permission states.

## Mode 1 — Claude Code (USB/BLE)

**Setup**

1. Install and launch Claude Buddy on your Flipper.
2. Install the companion Claude Code plugin:

   (bash)

   claude plugin marketplace add jxw1102/flipper-claude-buddy

   claude plugin install flipper-claude-buddy@flipper-claude-buddy

3. Start a Claude Code session. The plugin launches the Python host bridge automatically.

**Connection.** USB is used when the cable is plugged in; otherwise BLE. Switching is automatic.

**Button map**

| Button | Action |
|--------|--------|
| UP | Start / stop voice dictation |
| UP (hold) | Hold Space for voice input |
| LEFT | Interrupt Claude (Esc) |
| LEFT (hold) | Send Ctrl+C |
| RIGHT | Open slash command menu |
| RIGHT (hold) | Open info menu |
| OK | Submit Enter (⏎) |
| OK (hold) | Type "yes" and submit |
| DOWN | Send Down arrow (↓) |
| DOWN (hold) | Toggle mute |
| BACK | Send Backspace (⌫) |
| BACK (hold) | Exit app |

## Mode 2 — Claude Desktop (BLE)

This mode talks directly to the Claude Desktop app over BLE — no plugin, no Python bridge, pure BLE NUS.

**Setup**

1. On the Flipper, long-press **Right → MENU** and select **Claude Desktop (BLE)** in the top row.
2. In the Claude Desktop app: **Help → Troubleshooting → Enable Developer Mode** (adds a Developer menu).
3. **Developer → Open Hardware Buddy** and pick your Flipper from the scan list. Accept the macOS Bluetooth permission prompt the first time.

Once paired, Claude Desktop auto-reconnects whenever both sides are on.

**What you see on the Flipper**

- Running / waiting session counts and heartbeat status
- Recent transcript lines (scrollable under the TRANSCRIPT view)
- Permission prompts as they happen — Allow or Deny on-device

**Button map** (keystroke buttons are inactive here, since Claude Desktop doesn't take keystrokes over this protocol)

| Button | Action |
|---|---|
| RIGHT / RIGHT (hold) | Open info menu |
| OK | Allow a permission prompt |
| LEFT | Deny a permission prompt |
| DOWN (hold) | Toggle mute |
| BACK (hold) | Exit app |

The in-app **HELP** page reflects whichever mode is active.
