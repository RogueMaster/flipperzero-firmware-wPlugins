# Changelog

## 1.6

- Fix pressing OK while paused instantly starting a new game - OK now resumes the game, a new game can only be started from the game-over screen
- Fix a game restarted from the game-over screen reusing the previous game's falling piece instead of the freshly spawned one, which also broke the 7-bag piece sequence
- Fix pieces locking mid-air when a move or rotation coincides with a gravity step in the same game tick (floating pieces)
- Fix rotation kicks near the top of the field reading out of bounds and potentially corrupting memory
- Fix hard-dropped pieces sometimes stopping one row short and floating in the air
- Fix rotating a piece near the bottom of the playfield clipping it through the floor
- Fix a piece continuing to fall after it has landed and lines have been cleared
- Fix hard drop triggering on the pause and game-over screens

## 1.5

- Add hard drop on Up press

## 1.4

- Update for a newer firmware API (no functional changes)

## 1.3

- Update for a newer firmware API (no functional changes)

## 1.2

- Add 7-bag randomizer for a fair piece distribution
- Add next piece preview
- Add pause on Back press (hold Back to quit)
- Show the cleared lines count during the game

## 1.1

- Fix a possible freeze by moving screen updates outside the game state lock

## 1.0

- Initial release: classic Tetris with clockwise rotation and wall kicks
