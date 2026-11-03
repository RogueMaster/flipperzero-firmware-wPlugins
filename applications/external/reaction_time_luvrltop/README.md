# ReactionTime
A reaction time game for Flipper Zero, written in C. 

## Game idea:
Wait for the signal, then press OK as fast as you can.

## How to play
1. Press **OK** on the start screen.
2. Wait for **PRESS!!!** to appear. The delay until **PRESS!!!** is randomized (1.5-5 s).
3. Press **OK** as fast as you can. Your reaction time is shown in milliseconds.


## Controls
| Button | Action |
| --- | --- |
| OK | Start / react / play again |
| Left | Reset stats (in start screen) |
| Back | Return to start screen / exit app |

## Stats
Your results are saved to `apps_data/reaction_time/stats.txt` on the SD card. The file stores the total of all your reaction times and the number of rounds played. From these, your average reaction time is calculated and shown on the start screen. The stats persist between sessions until you reset them.

## Installing
Drag and drop the reaction_time.fap file into the 'apps/Games/' folder on your Flipper Zero.

## Building it yourself
Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

    ufbt launch

## License
MIT

## Screenshots
<img width="512" height="256" alt="Screenshot-20261103-001653" src="https://github.com/user-attachments/assets/9b527ef4-ea1a-456e-9395-4753ee9d00c5" />
<img width="512" height="256" alt="Screenshot-20261103-001702" src="https://github.com/user-attachments/assets/ea172394-d0b2-41d0-8af1-d7508fcaa694" />
<img width="512" height="256" alt="Screenshot-20261103-001708" src="https://github.com/user-attachments/assets/68445a4f-d978-4abf-8870-45a94e5e7e30" />
<img width="512" height="256" alt="Screenshot-20261103-001723" src="https://github.com/user-attachments/assets/b9fa9884-1e2e-46b3-b6fc-d53a50315f2a" />

