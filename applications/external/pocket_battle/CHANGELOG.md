# Changelog

All notable changes to Pokemon Yellow+ for Flipper Zero will be documented in this file.

## [2.0.0] - 2025-06-25

### 🎮 Major Release: Pokemon Yellow Integration

This is a major update that integrates authentic Pokemon data from the pokeyellow disassembly project, transforming the original showdown game into a comprehensive Pokemon experience.

### ✨ Added
- **Authentic Pokemon Data Integration**
  - 151 original Pokemon with accurate base stats from Pokemon Yellow
  - Real type effectiveness system (18x18 type chart)
  - Authentic move database with power, accuracy, and PP values
  - Individual Values (IVs) for stat variation

- **Enhanced Battle System**
  - Turn-based combat with speed-based move order
  - Status conditions: sleep, poison, burn, freeze, paralysis
  - Critical hit system with 1/16 chance
  - STAB (Same Type Attack Bonus) calculations
  - Authentic damage formulas from original Game Boy games

- **Team Management**
  - Support for teams of up to 6 Pokemon
  - Active Pokemon switching
  - Team healing functionality
  - Pokemon level progression system

- **New Game Features**
  - Wild Pokemon encounters with random generation
  - Trainer battles with different AI patterns
  - Enhanced main menu with multiple options
  - Pokedex browsing functionality

- **Technical Enhancements**
  - Memory optimized for Flipper Zero (4KB stack, <15KB RAM)
  - Proper Flipper Zero SDK integration with ufbt/fbt
  - Modular architecture for easy extension
  - Comprehensive error handling and memory management

- **New Files**
  - `pokemon_enhanced.h/c` - Core enhanced Pokemon system
  - `pokemon_integration.h/c` - Game state management
  - `enhanced_battle_screen.h/c` - Enhanced battle interface
  - `pokemon_main_enhanced.c` - New main application
  - `pokemon_yellow_data.c` - Extracted Pokemon data
  - `moves_data_manual.c` - Authentic move database
  - `Makefile`, `build.sh` - Flipper Zero build system
  - `FLIPPER_BUILD_GUIDE.md` - Comprehensive build documentation

- **UI Improvements**
  - Enhanced battle screen with HP bars
  - Move selection interface with PP display
  - Battle log system
  - Team status display
  - Pokemon sprites in XBM format

### 🔧 Changed
- Updated `application.fam` for enhanced features
- Modified `battle.c` to work with new system (sprites temporarily disabled)
- Renamed `pokemon_battle.c` to `pokemon_battle_original.c` to avoid conflicts
- Increased stack size to 4KB for enhanced features

### 🏗️ Build System
- Added ufbt/fbt compatibility
- Automatic dependency resolution
- Build scripts and comprehensive documentation
- Successfully builds 18KB FAP file

### 📊 Statistics
- **32 files changed** with 2329+ insertions
- **151 Pokemon** with authentic stats
- **50+ moves** with accurate data
- **18x18 type effectiveness** chart
- **Memory efficient**: ~10-15KB RAM usage
- **Build status**: ✅ SUCCESS

### 🎯 Performance
- Battle calculations: O(1) per turn
- Type effectiveness: Pre-calculated lookup table
- Memory usage: <15KB RAM, ~50KB Flash
- Battery friendly: Efficient rendering and low CPU usage

### 📚 Documentation
- Comprehensive README update
- Detailed build guide for Flipper Zero
- Integration documentation
- Troubleshooting guides
- Customization instructions

### 🙏 Credits
This major update integrates work from:
- **pret team** - pokeyellow disassembly project
- **HermeticCode** - Original showdown game foundation
- **Flipper Zero community** - SDK and development tools

---

## [1.0.0] - Original Showdown Release

### Initial Features
- Complete Gen 1 Pokemon roster
- Turn-based combat system
- Pokemon selection screen
- Health bars and battle animations
- Multiple moves per Pokemon
- Optimized for Flipper Zero 128x64 monochrome display

### Original Credits (HermeticCode)
- Nintendo/Game Freak - Original Pokemon designs
- Rogue Master/Malik - Community support
- Flipper Zero Team - Hardware platform
- Esteban Fuentealba/Kris Bahnsen - Inspiration
- Talking Sasquatch - Educational content
- Derek Jamison - Technical tutorials
- The Flipper community - Documentation and support

---

## Future Releases

### Planned Features
- [ ] More Pokemon sprites integration
- [ ] Sound effects from pokeyellow audio data
- [ ] World map system
- [ ] Persistent save system
- [ ] Online battle capabilities
- [ ] Pokemon Center functionality

### Version Numbering
- **Major.Minor.Patch** format
- Major: Significant feature additions or breaking changes
- Minor: New features, enhancements
- Patch: Bug fixes, small improvements
