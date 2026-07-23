## 1.1

- Flash more than one ESP board: "Install Firmware" now opens a board picker for the
  official Flipper WiFi Dev Board (ESP32-S2), an ESP32 WROOM board, or an ESP32-C5 board.
  Each board's firmware is bundled, so it stays an offline, no-computer flash.
- One-click flashing: each board also has an "(auto boot)" option that drops the board into
  download mode on its own (no holding BOOT and tapping RESET). The manual option stays for
  boards wired differently. Thanks to xMasterX for this, the C5 support, and the flasher fixes.
- The flasher no longer freezes if you press Back while it is waiting for the board.
- Note: the first launch (and each update) can take up to 3 minutes while the app unpacks
  its bundled firmware and game files to the SD card. The hourglass is the loader working.
- Firmware v11.

## 1.0

- Four games are now driven by plain-text content packs: Trivia, Would You Rather,
  Word Scramble, and Draw & Guess. Six packs per game ship inside the app, and your own
  packs in the per-game folders under /ext/apps_data/hotspot_arcade/packs/ still take precedence.
- New angry reaction emoji. Reactions are scoped to the people sharing your screen and
  now show who sent them.
- Lobby chat appears on the Flipper's Console, so the host can follow the chatter.
- Boards render correctly in phone browsers, including on iOS/Safari (Reversi, Connect
  Four, Tic-Tac-Toe).
- Rematch after an opponent leaves returns you to the lobby, and you can't challenge
  someone who is still on their win/lose screen.
- A captive-Wi-Fi popup now points you to open the game in your real browser, where the
  multiplayer connection is reliable.
- Firmware v11.

## 0.3

- Everything ships inside the app: the phone game bundle and trivia packs are bundled
  alongside the ESP firmware, so there is no SD card setup at all.
- Your own trivia packs and web bundle still work: drop them in
  /ext/apps_data/hotspot_arcade/ and they take precedence over the bundled ones.
- Nicknames are shown in all caps everywhere, for consistency across the phones and the
  Flipper.
- Flashing the board continues on its own once it reboots, so tapping RESET is the only step.
- Firmware v7.

## 0.2

- Ten games: Trivia, Would You Rather, Word Scramble, Reaction Duel, Connect Four,
  Tic-Tac-Toe, Dots & Boxes, Reversi/Othello, Drawing & guessing, and real-time Pong.
- Pick an emoji avatar; send emoji reactions that float up on every phone.
- Flash the ESP board on-device from the Flipper (no computer needed).
- Firmware identity + versioning, so an outdated board is offered an update.
- Redesigned broadcasting dashboard; every game is phone-driven and self-organizing.

## 0.1

- Initial release: offline multiplayer Trivia and Connect Four hosted from the Flipper
  and the ESP32-S2 WiFi board, with a streamed phone game client.
