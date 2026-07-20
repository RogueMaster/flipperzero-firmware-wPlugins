# Hotspot Arcade

Turn your Flipper Zero and the official ESP32-S2 WiFi dev board into an **offline
multiplayer arcade**. The Flipper hosts an open WiFi network; people nearby join it,
a captive page hands them into a game in their phone browser, and everyone plays
together over the local network. **No internet and no app install** — built for dead
zones like buses, planes, subways, and campsites.

The Flipper is the game master: it shows the lobby and live scoreboard and picks the
game. The ESP32 board is the referee: it runs the access point, serves the game to
phones, and keeps the real-time game state.

## Requires the ESP32-S2 WiFi board

This app needs the **official Flipper WiFi Dev Board (ESP32-S2)** on the GPIO header.
The board must run the Hotspot Arcade firmware — you can flash it **straight from the
Flipper** with the built-in **Install Firmware** option (no computer needed), or from a
computer with esptool.

## Ten games

- **Whole-group:** Trivia, Would You Rather, Word Scramble, Reaction Duel — everyone in
  the room plays at once, with a ready-up lobby and a live leaderboard.
- **1v1 duels:** Connect Four, Tic-Tac-Toe, Dots & Boxes, Reversi/Othello, and real-time
  Pong — challenge another player from your phone.
- **Drawing & guessing** — one player draws, everyone else guesses in a chat.

Pick an emoji avatar on the way in and send emoji reactions that float up on everyone's
screen. Everything is phone-driven: the Flipper just selects the game and watches the feed.

## How to use

1. Attach the ESP32-S2 board (flash its firmware via **Install Firmware** the first time).
2. **Start Session** — the dashboard shows **Broadcasting**.
3. Friends join the WiFi and open **192.168.4.1** in their browser, pick a nickname, and play.
4. **Games** selects the active game; **Scores** shows the live leaderboard.

## Responsible use

Hotspot Arcade runs an **open** WiFi access point and a captive page that serves only the
bundled game. It captures no credentials and is for fun among people who want to play.
Running an open AP may be restricted in some places (for example on aircraft) — only
operate it where that is allowed.

## Screenshots, source, and phone-client previews

Full documentation, the phone game-client screenshots, protocol, and build instructions
are on GitHub: **https://github.com/tarikbc/hotspot-arcade**
