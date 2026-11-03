# Reaction Time

A simple reaction time game for Flipper Zero. Wait for the signal, then press OK as fast as you can.

## How to play
1. Press **OK** on the start screen.
2. Wait for **PRESS!!!** to appear. The delay is random (1.5-5 s).
3. Press **OK** as fast as you can. Your reaction time is shown in milliseconds.

Pressing too early counts as a fail.

## Controls
- **OK**: start / react / play again
- **Left**: reset stats (start screen)
- **Back**: return to start screen / exit app

## Stats
Your results are saved to the SD card in apps_data/reaction_time/stats.txt. The file stores the total of all your reaction times and the number of rounds played. From these, your average reaction time is calculated and shown on the start screen.