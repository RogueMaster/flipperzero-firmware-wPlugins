# Lights Out Flipper Zero (LOFZ)

**Lights Out Flipper Zero (LOFZ)** is a Star Wars-inspired puzzle game for the Flipper Zero, based on the classic "Lights Out" game. As the last Jedi, you must toggle a 17-cell grid of lights to turn them all off, battling the evil Sith Lord Flipper Zero, who floods the galaxy with light. Built with AI assistance from Copilot and Grok 3 (xAI), this game evolved from a JavaScript prototype showcasing Bi-Sectional Diffusion Models in a 17-cell grid to a polished C implementation for Flipper Zero.

## Lore
A long time ago in a galaxy far, far away... The evil Flipper Zero, Sith Lord, has covered the galaxy in light! No one has slept in ages. You are the last Jedi, and your mission is to turn the lights out and vanquish Flipper! Use the D-pad to move and OK to select. The Back button (2x+) exits. The game goal: turn all boxes dark. If all tiles are lit, the Sith wins. May the Force be with you!

## Features
- **Lights Out Puzzle**: Toggle a 4x4 grid (plus an offset block) to turn all cells off. Toggling a cell flips it and its neighbors; the offset block affects four specific cells.
- **AI Opponent**: Sith Lord Flipper Zero makes strategic moves, choosing lit cells closest to your cursor, with 1-4 moves per turn based on a sequence.
- **Star Wars Aesthetic**: Features a scrolling intro crawl, a 16x16 Sith Flipper mascot (1bpp bitmap) that bounces or moves under stress (<20% cells off or score > 981), and thematic win/loss screens.
- **Special Screens**:
  - **Win Screen**: "Victory! The Jedi Triumph!" on success.
  - **Dead Screen**: "You Died! Flipper Wins!" if all cells lit.
  - **Mischief Screen**: "I solemnly swear I'm up to no good!" with a Marauder’s Map vibe.
  - **Secret Code Screen**: Lists input sequences for special screens.
  - **Leaderboard Screen**: Shows top 10 scores (wins and flips).  *Currently doesn't corretly save the data to the file*.
- **Leaderboard**: Stores top 10 entries (wins, total flips) in `/ext/lofz_leaderboard.txt`.
- **UI Enhancements**: 3D-effect board, scrolling Flipper phrases (e.g., "Flipper is Thinking..."), fade transitions, and score/wins display.
- **Audio Toggle**: Enable/disable audio with 3 Left presses (audio not implemented).
- **Secret Codes**: 9-button sequences unlock special screens (see Controls).
- **Optimized for Flipper Zero**: Runs at ~40 FPS, using Furi APIs for GUI, input, and storage on a 128x64 display.
- **Development**: Transmuted from JavaScript via Copilot, with UI polish and bug fixes by Grok 3 (xAI).

## Game Modes
- **Lights Out Flipper Zero**: Toggle a 4x4 grid (plus offset block) to turn all cells off, competing against Flipper’s AI.

## States
- **Loading**: Shows "Loading..." for 7.28 seconds.
- **PressToPlay**: Displays "Press OK to Play".
- **IntroCrawl**: Scrolling intro or leaderboard text.
- **IntroFade**: Fades into gameplay.
- **PlayerTurn**: Move cursor and toggle cells.
- **FlipperPopup**: Shows Flipper’s taunting phrase.
- **FlipperTurn**: Flipper AI makes moves.
- **Finished**: Exits game loop.
- **LostNulls**: Shows loss text when all cells lit.
- **LostFade**: Fades to `LostNulls`.
- **LostMenu**: Asks "You Died, Play Again?".
- **WinScreen**: Shows win text.
- **Credits**: Scrolling credits.
- **SecretCode**: Displays secret code sequences.
- **DeadScreen**: Shows "You Died!" text.
- **WinFade**: Fades to `WinScreen`.
- **MischiefScreen**: Shows "I solemnly swear..." text.
- **WinMenu**: Asks "Victory, Play Again?".

## Controls
### Loading, IntroFade, LostFade, WinFade, Finished, ExitScreen
- No inputs.

### PressToPlay, IntroCrawl
- **OK (Press)**: Start game (`PressToPlay`) or skip to `IntroFade` (`IntroCrawl`).
- **Left (Press, 3x within 1.8s)**: Toggle audio. *May not work*
- **Up (Press, 10x within 1.8s)**: Show Leaderboard screen. *May not work*

### PlayerTurn
- **Up (Press)**: Move cursor up (to grid index 4 from offset block).
- **Down (Press)**: Move cursor down (to grid index 12 from offset block).
- **Left (Press)**: Move cursor left (to grid index 3 from offset block).
- **Right (Press)**: Move cursor right (to offset block from rightmost column).
- **OK (Press)**: Toggle cell and neighbors, check win/loss, go to `FlipperPopup`, `WinFade`, or `LostFade`.
- **Back (Press, 2x within 1.8s)**: Exit to `ExitScreen`.
- **Right (Press, 3x within 1.8s)**: Enter `Credits` (if move count ≥ 4 or 3-minute cooldown).
- **Left (Press, 3x within 1.8s)**: Toggle audio. *I haven't gotten audio to work yet, this is a hopeful upgrade when I do*
- **Up (Press, 10x within 1.8s)**: Show Leaderboard screen. *If leaderboard load failed, will replay intro*

### FlipperPopup, FlipperTurn
- No inputs (auto-transitions).

### LostMenu, WinMenu
- **Up or Right (Press)**: Select "Yes" to play again.
- **Down or Left (Press)**: Select "No" to exit.
- **OK (Press)**: Play again (`PlayerTurn`) or exit (`ExitScreen`).
- **Left (Press, 3x within 1.8s)**: Toggle audio.
- **Up (Press, 10x within 1.8s)**: Show Leaderboard screen.

### Credits
- **OK (Press)**: Return to `IntroCrawl`, restore game state.
- **Left (Press, 9x within 1.8s)**: Enter `SecretCode`, cycle codes.
- **Sequence (9 presses)**:
  - **UUUDURUDR**: Show `WinScreen` (Up, Up, Up, Down, Up, Right, Up, Down, Right). *Was used to ensure win screen looks correct*
  - **UUUDULUDL**: Show `DeadScreen` (Up, Up, Up, Down, Up, Left, Up, Down, Left).  *Was used to ensure death screen looks correct*
  - **DUDUDUDUO**: Show `MischiefScreen` (Down, Up, Down, Up, Down, Up, Down, Up, OK).
                         *Resembles looking up and down checking the Map of Mishief becuase the only other codes here are "**cheat codes**"*
  - Any other + OK: Return to `IntroCrawl`. *Anti Mix-Clicks, **MUST CLICK PROPERLY & CLEANLY TO CHEAT***

### SecretCode, DeadScreen, MischiefScreen, WinScreen
- **OK (Press)**: Return to `IntroCrawl`.
- **Left (Press, 3x within 1.8s)**: Toggle audio. *May Not Work*
- **Up (Press, 10x within 1.8s)**: Show Leaderboard screen. *May Only Show Intro*

## Building and Running
1. Clone this repository to your Flipper Zero development environment.
2. Compile `lofz.c` using the Flipper Zero firmware SDK.
3. Deploy to your Flipper Zero and launch from the apps menu.

## Screenshots
![LOFZ Loading Screen](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172625.png)
![LOFZ Agreement Screen](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172632.png)
![LOFZ Intro Crawl Screen](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172639.png)
![LOFZ Inital Game Screen](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172646.png)
![LOFZ Gameplay (User Turn)](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172651.png)
![LOFZ Gameplay (Flipper Turn)](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172714.png)
![LOFZ Credit Crawl Screen](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172724.png)
![LOFZ Away-Message](https://raw.githubusercontent.com/DigiMancer3D/LOFZ/refs/heads/main/Screenshots/Screenshot-20250718-172750.png)

## Notes
- First Flipper Zero game by 3DPihl, transmuted from JavaScript using Copilot, with UI enhancements and bug fixes by Grok 3 (xAI).
- Optimized for Flipper Zero’s 128x64 display and 5-button input.
- Saves leaderboard to `/ext/lofz_leaderboard.txt`.
- Special screens add replayability and Star Wars charm.

## Contributing
Submit pull requests to add features, refine AI, or enhance the Star Wars theme. May the Force be with you!
