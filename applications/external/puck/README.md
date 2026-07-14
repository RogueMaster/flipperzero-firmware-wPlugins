# mitzi-puck
<img alt="Main Puck Girl Screen"  src="screenshots/MainScreen.png" width="40%" />
A simple Flipper Zero arcade chase game. The player controls a pie-shaped character ("Puck girl") through a maze, collecting dots while avoiding ghosts. Power pills temporarily make ghosts vulnerable.
Like in the classic game, each ghost has distinct personality: one targets directly, another one patrols. 

## Usage
- **Arrow Keys**: Move Puck-girl through the maze
- **OK Button**: Restart game (when game over or won)
- **Back Button**: Pauses game or (when held) exits

On the right, the user can track the lives of puck-girl &mdash; initially there are 3 filled hearts which become empty for each death of puck-girl. Underneath, there is a score-tracker. The theoretically maximal possible high-score is `4*2*200+4*50+ (12*7-2-4)*10 = 2580` &mdash; if the users kills both ghosts in each power-up phase. 

### Scores
- 10 points for normal dot
- 50 for power pills
- 200 for eating a ghost - ghosts respawn after the power-up-period of 5s

## Remarks on the implementation
Drawing of the screen occurs back to front:
1. **Maze Background:** Pre-rendered static (full-screen) image
2. **Pellets:** Dots and power pills
3. **Puck-girl:** Animated sprite with directional variants
4. **Ghosts:** 2-frame wobbeling
5. **HUD (= head up display):** Score, lives, power timer, version
6. **Navigation Hints** 
7. **Modals:** Pause/died/game over/win messages

## Version history
See [changelog.md](changelog.md)
