# Hunter Killer - Flipper Zero Submarine Game

A submarine warfare simulation game ported from Pico-8 to Flipper Zero using the Flipper Zero Game Engine.

## Features

- **Realistic submarine physics** - Navigate using heading and velocity controls
- **Procedural terrain generation** - Diamond-square algorithm creates unique underwater landscapes
- **Advanced sonar system** - Ping to discover and map terrain 
- **Torpedo warfare** - Fire torpedoes to destroy terrain obstacles
- **Dual game modes** - Switch between Navigation and Torpedo modes
- **Memory optimized** - Efficient terrain and entity management for Flipper Zero

## Controls

- **D-pad Left/Right**: Adjust submarine heading (steering)
- **D-pad Up/Down**: Control thrust (forward/reverse)
- **OK Button**: 
  - Navigation mode: Send sonar ping
  - Torpedo mode: Fire torpedo
- **Back Button**:
  - Short press: Toggle between NAV/TORPEDO modes  
  - Long press: Exit game

## HUD Display

- **V**: Current velocity
- **H**: Current heading (0.0-1.0)
- **NAV/TORP**: Current mode
- **T**: Torpedo count (used/available)

## Gameplay

1. **Navigation**: Use D-pad to control submarine movement with realistic physics
2. **Sonar Mapping**: Press OK in NAV mode to ping and discover terrain
3. **Terrain Collision**: Submarine stops when hitting discovered land masses
4. **Torpedo Combat**: Switch to TORP mode and fire torpedoes at terrain
5. **Resource Management**: Limited torpedo supply encourages strategic play

## Building

### Requirements
- Python 3 with ufbt installed: `pip install ufbt`
- Flipper Zero or Flipper Zero simulator

### Build Commands
```bash
make build    # Build the game
make launch   # Build and launch on connected Flipper Zero  
make clean    # Clean build artifacts
make help     # Show all available targets
```

## Technical Details

- **Engine**: Flipper Zero Game Engine with entity-component system
- **Terrain**: Optimized diamond-square algorithm (65x65 chunks)
- **Memory**: Efficient collision detection and sonar chart storage
- **Rendering**: Monochrome graphics optimized for 128x64 display

## Architecture

- **game.c/h**: Main game logic and submarine entity
- **terrain.c/h**: Procedural terrain generation and collision detection
- **Makefile**: Build system with convenient targets
- **application.fam**: Flipper Zero app configuration

## Original Pico-8 Version

This is a port and enhancement of the hunterkiller submarine game originally written for Pico-8. The Flipper Zero version includes:

- Improved entity-component architecture
- Memory optimization for embedded constraints  
- Enhanced visual feedback and UI
- Better collision detection and physics
- Structured codebase for maintainability

## License

[Include appropriate license information]