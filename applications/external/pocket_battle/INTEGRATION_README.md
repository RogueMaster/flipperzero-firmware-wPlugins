# Pokemon Yellow+ Integration Guide

This document explains how the pokeyellow disassembly data has been integrated into your Flipper Zero showdown game.

## Overview

The integration combines authentic Pokemon data from the pokeyellow disassembly project with your existing Flipper Zero Pokemon battle game, creating a more authentic and feature-rich experience.

## Key Integration Components

### 1. Data Extraction (`pokemon_data_extractor.py`)
- Extracts Pokemon names, base stats, and types from pokeyellow assembly files
- Converts move data including power, accuracy, and PP
- Generates C code compatible with your Flipper Zero project

### 2. Enhanced Pokemon System (`pokemon_enhanced.h/c`)
- Extended Pokemon structure with authentic stats from pokeyellow
- Individual Values (IVs) for stat variation
- Status conditions (poison, burn, sleep, etc.)
- Type effectiveness system
- Authentic stat calculation formulas

### 3. Integration Layer (`pokemon_integration.h/c`)
- Game state management
- Team management (up to 6 Pokemon)
- Wild and trainer battle initiation
- Save/load functionality framework

### 4. Enhanced Battle System (`enhanced_battle_screen.h/c`)
- Turn-based battle mechanics
- Move selection interface
- HP bars and status display
- Battle log system

## File Structure

```
showdown-current/
├── pokemon_enhanced.h/c          # Enhanced Pokemon data structures
├── pokemon_integration.h/c       # Game integration layer
├── enhanced_battle_screen.h/c    # Enhanced battle interface
├── pokemon_yellow_data.c         # Generated Pokemon data
├── moves_yellow_data.c          # Generated moves data
├── pokemon_main_enhanced.c       # Enhanced main application
├── sprites/                      # Pokemon sprite files
│   ├── pokemon_*.xbm            # Individual Pokemon sprites
│   └── pokemon_sprites.h        # Sprite access header
└── application_enhanced.fam      # Enhanced app configuration
```

## Key Features Added

### Authentic Pokemon Data
- 151 original Pokemon with accurate base stats
- Authentic move data with power, accuracy, and PP
- Type effectiveness system matching original games
- Individual Values for stat variation

### Enhanced Battle System
- Turn-based combat with speed-based move order
- Status conditions (poison, burn, paralysis, etc.)
- Critical hits and type effectiveness
- STAB (Same Type Attack Bonus)
- PP (Power Points) system for moves

### Team Management
- Support for teams of up to 6 Pokemon
- Active Pokemon switching
- Team healing functionality
- Pokemon level progression

### Game Mechanics
- Wild Pokemon encounters
- Trainer battles with different AI
- Experience and leveling system
- Growth rate calculations

## Building and Installation

### Prerequisites
- Flipper Zero SDK
- Python 3 with PIL (for sprite conversion)
- pokeyellow disassembly project

### Build Process
```bash
# Extract data from pokeyellow
make extract_data

# Build the enhanced application
make build

# Install to Flipper Zero
make install
```

## Code Integration Mechanics

### 1. Data Flow
```
pokeyellow/*.asm → python extractors → C data files → Flipper Zero app
```

### 2. Memory Management
- Dynamic allocation for Pokemon structures
- Proper cleanup in all exit paths
- Stack size increased to 4KB for enhanced features

### 3. Type System
```c
// Enhanced type handling with dual types
typedef struct {
    PokemonType type1;
    PokemonType type2;  // For dual-type Pokemon
    // ... other fields
} EnhancedPokemon;
```

### 4. Battle Mechanics
```c
// Damage calculation with type effectiveness
uint16_t damage = base_damage * type_effectiveness * stab_bonus * critical_hit;
```

## Customization Options

### Adding New Pokemon
1. Add data to `pokemon_data_extractor.py`
2. Regenerate data files with `make extract_data`
3. Rebuild application

### Modifying Battle Mechanics
- Edit `enhanced_pokemon_calculate_damage()` for damage formulas
- Modify `type_chart[][]` for type effectiveness
- Adjust status condition logic in `enhanced_pokemon_can_move()`

### Adding New Features
- Extend `PokemonGameState` for new game data
- Add new screens following the pattern in `enhanced_battle_screen.c`
- Update main menu in `pokemon_main_enhanced.c`

## Performance Considerations

### Memory Usage
- Each Pokemon: ~200 bytes
- Battle state: ~500 bytes
- Sprite data: ~1KB per sprite
- Total RAM usage: ~10-15KB

### CPU Usage
- Battle calculations: O(1) per turn
- Type effectiveness: O(1) lookup
- Sprite rendering: Optimized for Flipper Zero display

## Debugging and Troubleshooting

### Common Issues
1. **Build failures**: Check Flipper SDK path in Makefile
2. **Data extraction errors**: Verify pokeyellow path in extractor scripts
3. **Memory issues**: Monitor stack usage, increase if needed
4. **Display problems**: Check canvas drawing bounds

### Debug Logging
```c
FURI_LOG_I("Pokemon", "Debug message: %s", pokemon->name);
```

## Future Enhancements

### Planned Features
- More authentic sprite rendering from Game Boy data
- Sound effects from pokeyellow audio data
- Map system integration
- Item system
- Pokemon Center functionality

### Extension Points
- `pokemon_integration.h` - Add new game modes
- `pokemon_enhanced.h` - Extend Pokemon data structure
- `enhanced_battle_screen.c` - Add new battle animations

## Contributing

When adding features:
1. Follow existing code style
2. Add appropriate error handling
3. Update documentation
4. Test on actual Flipper Zero hardware
5. Consider memory constraints

## License and Credits

- Original pokeyellow disassembly: pret team
- Flipper Zero showdown game: HermeticCode
- Integration layer: Enhanced version

This integration preserves the authentic Pokemon experience while adapting it for the Flipper Zero platform's constraints and capabilities.
