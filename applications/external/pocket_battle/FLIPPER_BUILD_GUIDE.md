# Pokemon Yellow+ Flipper Zero Build Guide

This guide explains how to build and install the enhanced Pokemon game on your Flipper Zero device.

## Prerequisites

### Option 1: ufbt (Recommended - Lightweight)
```bash
# Install ufbt (micro Flipper Build Tool)
pip install ufbt

# Update ufbt
ufbt update
```

### Option 2: Full Flipper Zero SDK
```bash
# Clone the full SDK (larger download)
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
./fbt
```

## Project Structure

Your showdown-current directory should contain:
```
showdown-current/
├── application.fam              # App configuration
├── pokemon_main_enhanced.c      # Main application
├── pokemon_enhanced.h/c         # Enhanced Pokemon system
├── pokemon_integration.h/c      # Integration layer
├── enhanced_battle_screen.h/c   # Battle interface
├── pokemon_yellow_data.c        # Generated Pokemon data
├── moves_data_manual.c          # Move data
├── pokemon_10px.png            # App icon
├── sprites/                     # Pokemon sprites
├── build.sh                     # Build script
└── Makefile                     # Build automation
```

## Building the Application

### Method 1: Using the Build Script (Easiest)
```bash
cd /home/tim/showdown-current
./build.sh
```

### Method 2: Using Make
```bash
cd /home/tim/showdown-current
make all
```

### Method 3: Direct ufbt/fbt Commands
```bash
cd /home/tim/showdown-current

# Using ufbt (recommended)
ufbt build

# Or using fbt (if you have full SDK)
fbt build
```

## Installing to Flipper Zero

### Connect Your Flipper Zero
1. Connect Flipper Zero via USB
2. Ensure it's in normal mode (not DFU mode)

### Install the App
```bash
# Using ufbt
ufbt install

# Using make
make install

# Using fbt
fbt install
```

### Launch the App
```bash
# Build and launch directly
ufbt launch

# Or manually navigate on Flipper:
# Apps → Games → Pokemon Yellow+
```

## Troubleshooting

### Build Errors

#### "ufbt/fbt not found"
```bash
# Install ufbt
pip install ufbt

# Or check PATH
echo $PATH
which ufbt
```

#### "application.fam not found"
```bash
# Make sure you're in the right directory
cd /home/tim/showdown-current
ls -la application.fam
```

#### "Source file not found"
```bash
# Check if all required files exist
ls -la *.c *.h
```

#### Memory/Stack Issues
If you get stack overflow errors:
1. Edit `application.fam`
2. Increase `stack_size=4 * 1024` to `stack_size=8 * 1024`

### Runtime Errors

#### App Crashes on Startup
1. Check Flipper Zero logs:
   ```bash
   ufbt cli
   # Then: log
   ```
2. Reduce features if memory is tight
3. Check for null pointer dereferences

#### Display Issues
1. Verify canvas drawing bounds (128x64 max)
2. Check font usage (FontPrimary, FontSecondary)
3. Ensure proper canvas_clear() calls

## Development Workflow

### 1. Edit Code
```bash
# Edit your C files
nano pokemon_main_enhanced.c
```

### 2. Build and Test
```bash
# Quick build and launch
ufbt launch
```

### 3. Debug
```bash
# Start debug session
ufbt debug

# Or check logs
ufbt cli
# Then type: log
```

### 4. Format Code
```bash
make format
```

## File-by-File Build Dependencies

### Core Files (Required)
- `application.fam` - App configuration
- `pokemon_main_enhanced.c` - Entry point
- `pokemon_enhanced.h/c` - Core Pokemon system
- `pokemon_integration.h/c` - Game integration
- `pokemon_yellow_data.c` - Pokemon data

### UI Files (Optional but Recommended)
- `enhanced_battle_screen.h/c` - Battle interface
- `pokemon_10px.png` - App icon
- `sprites/` - Pokemon sprites

### Legacy Files (Fallback)
- `pokemon.c/h` - Original Pokemon system
- `battle.c/h` - Original battle system
- `menu.c/h` - Original menu system

## Build Configurations

### Debug Build
```bash
ufbt build --extra-define=DEBUG=1
```

### Release Build
```bash
ufbt build --extra-define=NDEBUG=1
```

### Minimal Build (if memory is tight)
Edit `application.fam`:
```python
App(
    # ... other settings ...
    stack_size=2 * 1024,  # Reduce stack
    # Remove optional features
)
```

## Performance Optimization

### Memory Usage
- Each Pokemon: ~200 bytes
- Battle state: ~500 bytes  
- Total RAM: ~10-15KB
- Flash: ~50KB

### CPU Optimization
- Battle calculations: O(1)
- Type effectiveness: Lookup table
- Sprite rendering: Optimized for 128x64

### Battery Life
- Efficient rendering (only update when needed)
- Sleep between frames
- Minimal CPU usage in menus

## Advanced Features

### Custom Pokemon Data
1. Edit `pokemon_data_extractor.py`
2. Run `python3 pokemon_data_extractor.py`
3. Rebuild app

### Adding New Screens
1. Follow pattern in `enhanced_battle_screen.c`
2. Add to main menu in `pokemon_main_enhanced.c`
3. Update `application.fam` if needed

### Sprite Integration
1. Convert sprites to XBM format
2. Add to `sprites/` directory
3. Update `pokemon_sprites.h`

## Testing Checklist

Before releasing:
- [ ] App builds without errors
- [ ] App installs on Flipper Zero
- [ ] App launches without crashing
- [ ] Basic navigation works
- [ ] Battle system functions
- [ ] Memory usage is reasonable
- [ ] No stack overflows
- [ ] UI elements display correctly

## Getting Help

### Common Resources
- [Flipper Zero Developer Docs](https://developer.flipper.net/)
- [ufbt Documentation](https://pypi.org/project/ufbt/)
- [Flipper Zero Discord](https://discord.gg/flipper)

### Debug Information
When asking for help, include:
1. Build tool version (`ufbt --version`)
2. Flipper firmware version
3. Full error messages
4. Your `application.fam` file
5. Build logs

### Log Collection
```bash
# Collect build logs
ufbt build 2>&1 | tee build.log

# Collect runtime logs
ufbt cli
# Then: log > runtime.log
```

This build system is specifically designed for the Flipper Zero's constraints and build tools, ensuring your Pokemon game will compile and run properly on the device.
