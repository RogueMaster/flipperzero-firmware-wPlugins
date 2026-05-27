## v0.6

- Fixed connection/disconnection state handling in Bridge mode.
- Fixed a memory leak in the Flipper app.


## v0.5

- New **Claude Desktop (BLE)** mode: talks directly to the Claude Desktop app over Anthropic's Hardware Buddy protocol. No plugin needed.
- Linux support for the host bridge plugin — thanks to @DanilaE for the contribution!
- Info menu relabels: **Claude Code (USB/BLE)** / **Claude Desktop (BLE)**.
- Help and Transcript pages are now mode-aware.
- Changed Flipper app category from USB to Bluetooth.


## v0.4

- Info menu (Hold ►): Help, Transcript, Plan/Code Mode, About.
- Transcript scrolling with Page Up/Down and jump-to-top/bottom.
- Flipper notifications for tool failures and elicitation prompts.

## v0.3

- Auto-discover slash commands from user/project commands, skills, and enabled plugins.
- Selected menu commands are promoted to the top of the list for convenient re-use.

## v0.2

- Routed remote input to the active runner session, including the correct terminal tab.
- Added Up-button long-press support for Claude voice input.
- Fixed BLE signal bars in the header.
- Fixed slash-command menu refresh so the first selected command matches the item shown on screen.

## v0.1

- Initial release with physical remote control, haptic feedback, USB/BLE transport, and Claude Code plugin integration.
