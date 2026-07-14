# mitzi-mancala
<img alt="Main Mancala Screen"  src="screenshots/mitzi-mancala-main-v0.1.png" width="40%" />

## Usage
* **Left / Right-Button.** Moves cursor between the 6 user pits
* **OK** picks up stones from selected pit and perform the distributions of the stones according to the rules
* Holding **Back** quits the App.
Further features:
* Visual cursor to select your moves
* Status messages for game events
* Flipper's turn is delayed for visibility of the move

## Game rules for standard mancala ("Kalah")
1. Pick up all stones from selected pit.
2. Move counterclockwise placing 1 stone per pit.
3. Skip opponent’s store; place in own store.
4. If last stone lands in own store → take another turn.
5. If last stone lands in an empty pit on your side → capture that stone + opposite pit’s stones into your store.
6. Game ends when one side is empty → other side’s remaining stones flow to their store.
7. Store counts determine winner.

## Simple heuristics
The computer player ('AI' is a bit much) tries all 6 possible moves and decides for the one with the highest score.
It is based on the following (usual) scoring scheme:
- Extra turns (highest priority): +100 points
- Captures (medium priority): +50 points plus captured stones
- Points gained: +10 per stone added to store

## Version history
See [changelog.md](changelog.md)
