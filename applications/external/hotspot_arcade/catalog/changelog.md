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
