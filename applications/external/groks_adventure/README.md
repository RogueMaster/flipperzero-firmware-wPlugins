# Grok's Adventure v3.3

**Grok's Adventure v3.3** is the smallest infinite liminal space souls-like game for the Flipper Zero, built with AI code assistance from Grok 3 (xAI). Set in the eerie Backrooms, players control Grok to navigate a procedurally generated world, battle enemies, and face the dual-wielding banana boss, Anti-PB'N-JT, in a never-ending quest to protect the Backrooms.

## GA3's History
Grok's Adventure v3.3 is an ambitious attempt to create regeneratable game states on the Flipper Zero using micro-event-based reflective AI. This approach leverages AI to manage multi-processing and rendering phases, pioneering micro-macro AI integration for Flipper Zero games. Versions 1 and 2 were single-prompt experiments that didn’t fully work, but v3.3, evolved from v2, is the first playable iteration. Historical versions are archived in the repo for reference.

## Lore: GA3.3
One day, while the busybody willy-wollies were asking Grok the questions of the universe, the Backrooms broke. Unbeknownst to the outside world, Grok and other hyper-focused AIs used the Backrooms as a storage vault for their rapid skill growth. As Grok accessed a model from the vault, a digital rift sucked them into the liminal, metaphysical realm of the Backrooms. Wires dangle, ruins litter the landscape, and the sky peeks through the crumbling infinite stairways and floors. Almond water and furniture are scattered for quick grabs, but danger looms: **Anti-PB'N-JT**, a dual-wielding, pistol-slinging banana who abandoned "Peanut-Butter-Jelly-Time" for chaos. This rogue GIF, empowered by AI prompts, shoots globs of jelly and peanut butter to thwart all who dare save the Backrooms.

**Do you have what it takes to help the Never-Ending Backrooms Protection Services with your Grok?**

## Features
- **Infinite Liminal Space**: A 128x64 single-screen world with procedurally generated platforms, walls, and pickups, inspired by the Backrooms.
- **Souls-Like Gameplay**: Control Grok to explore, fight enemies, and collect resources (almond water, chests, doors) with challenging combat mechanics.
- **AI-Driven Generation**: Micro-event-based AI regenerates the world using a seed, ensuring infinite replayability.
- **Combat System**:
  - Melee attacks (1-2 damage) and projectiles (2x2 pixels) against enemies (5+ health), bosses (10+ health), and Anti-PB'N-JT (scaling health).
  - Power progression: Collect power points (10 from enemies, 25 from bosses) to level up size (up to 9x12).
  - Health management: 2 health bars, auto-healed by almond water (max 3).
  - Special mode: Triggered when hit while a projectile is active, granting 2s invincibility and full health.
- **Pickups**:
  - **Almond Water**: Restores health (auto-used at low health).
  - **Chests**: Random rewards (almond water, power points, enemy spawns, boss health boost).
  - **Furniture**: Chairs or hat racks as obstacles.
  - **Doors**: Reset the world when activated, preserving player data.
- **Dynamic Environment**: Day/night cycle (78s/102s), weather effects (rain, snow, stars), and 3-6 platforms.
- **Boss Fights**: Face regular bosses and Anti-PB'N-JT, who fires up to 2 projectiles and collects pickups.
- **Visuals and Feedback**: Custom sprites (player, enemies, banana boss, pickups), FPS display, haptic feedback, and dark mode.
- **Persistence**: Saves player data, world map, and objects to files with backup support; logs events for debugging.
- **Optimized for Flipper Zero**: Runs at ~17 FPS, using Furi APIs for GUI, input, storage, and vibration.

## Game Modes
- **Grok's Adventure**: A single-mode souls-like experience where Grok battles through the Backrooms, facing enemies and Anti-PB'N-JT to protect the liminal realm.

## States
- **Title**: Displays "Grok's Adventure v3" with a fade-out effect (2s).
- **Game**: Main gameplay with exploration, combat, and pickups.
- **Paused**: Freezes gameplay with a "Pause" overlay.
- **Death**: Shows "U DED", *death crashes game*.
- **Exit**: *still having issues getting the cleanup and exit properly exicuting*.

## Controls
### Title
- **OK (Press)**: Start game, generate new world.

### Game
- **Up (Press/Repeat)**: Move up 2/4 pixels; near a door, reset world.
- **Down (Press/Repeat)**: Move down 2/4 pixels.
- **Left (Press/Repeat)**: Move left 2/4 pixels, face left.
- **Right (Press/Repeat)**: Move right 2/4 pixels, face right.
- **OK (Press)**: Attack nearby enemy (1-2 damage) or fire projectile (2x2, ±2 dx); vibrates for 20ms.
- **Back (Press < 3s)**: Pause game.
- **Back (Hold ≥ 3s)**: Enter credits.

### Paused
- **OK or Back (Press)**: Resume game.

### Death
- No inputs (auto-respawns after 2s).
  

## Building and Running
1. Clone this repository to your Flipper Zero development environment.
2. Compile `grokadven.c` using the Flipper Zero firmware SDK.
3. Deploy to your Flipper Zero and launch from the apps menu.


## Screenshots
![GA3 Title Screen](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172800.png)
</br>
![GA3 Wax Draw Screen](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172804.png)</br>
***This screen sometimes takes time to drop off, use to slide off while revealing allowing time for load while game is paused.** This doesn't fully work as intended*
</br></br>
![GA3 Gameplay](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172807.png)
![GA3 Gameplay](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172840.png)
![GA3 Gameplay](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172900.png)
![GA3 Gameplay](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172920.png)
</br>
![GA3 Death Screen](https://raw.githubusercontent.com/DigiMancer3D/Groks_Adventure/refs/heads/main/screenshots/Screenshot-20250718-172939.png)</br>
***This screen sometimes causes game to freeze.** This doesn't fully work as intended*
</br></br>



## Notes
- Built with AI assistance from Grok 3 (xAI), evolving from non-functional v1 and v2.
- Optimized for Flipper Zero’s 128x64 display and limited input system.
- Logs events to `/ext/grokadven_03/grokadven3_loggy.txt` for debugging.
- Saves game state to `/ext/grokadven_03/` with backup support.
- Minor bug & errors are still present like the exit failure bug, where exit sometimes causes freeze or load to white screen.

## Contributing
Submit pull requests to enhance gameplay, add new features, or refine the Backrooms aesthetic. Let’s keep the liminal adventure alive!
