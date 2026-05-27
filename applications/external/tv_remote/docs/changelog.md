## v1.1

- New setting: **L/R Hold** — optionally make holding Left/Right send the same button as a short press (directional) instead of the alternate action
- New setting: **U/D Hold** — same as above for Up/Down
- New setting: **Hold TX** — choose between **Single** (one IR burst on hold, then stop) and **Continuous** (keep transmitting until the button is released)
- Button Map now reflects the current L/R Hold and U/D Hold settings so displayed labels stay accurate
- Fixed: app settings file moved from infrared/ to apps_data/flipper_tv_remote/ (correct isolated storage location)

## v1.0

Initial release.

- Multiple named remotes — save, use, and delete as many remotes as you like, each stored as a separate .ir file on the SD card
- Learn mode — record up to 14 IR buttons (Power, Mute, Vol+, Vol−, Ch+, Ch−, Up, Down, Left, Right, OK, Back, Home, Play/Pause) from any IR remote
- Concentric-circle remote UI — visual ring layout showing the active button section when a signal is sent
- Dual-action button mapping — short press and hold trigger different IR commands on the d-pad and OK button
- Double-tap Back to send the Power signal
- Button swap setting — optionally swap short press and hold actions so that Vol/Ch are on short press and directional signals are on hold
- Screen orientation setting — choose Vertical (portrait) or Horizontal (landscape); preference is saved to SD card and restored on next launch
- Button Map — in-app scrollable reference showing every button's press and hold action
- About page — author and licence info
- App icon (10×10 monochrome)
- Settings and remotes persist across restarts via SD card storage
- Saved .ir files use the standard Flipper IR format and are compatible with the built-in Infrared app
