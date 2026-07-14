# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Flipper Zero external application (FAP) called "Flipper Printer" that combines a coin flip game with thermal printer functionality. The application features a menu-driven interface with thermal printer integration via UART communication.

## Build Commands

```bash
# Using the provided Makefile (preferred)
make build              # Build the application
make upload             # Build and upload to Flipper
make clean              # Clean build artifacts
make debug              # Build debug version
make format             # Format source code
make lint               # Lint source code
make vscode             # Generate VS Code configuration
make install-ufbt       # Install uFBT build tool

# Direct uFBT commands (if installed)
ufbt                    # Build current app
ufbt launch             # Build and run on Flipper
ufbt -c                 # Clean build

# Direct FBT commands (from firmware repo)
./fbt fap_flipper_printer
./fbt fap_flipper_printer upload
./fbt -c
```

## Architecture

### Application Structure
- **App ID**: `flipper_printer`
- **Entry Point**: `flipper_printer_main()` in `flipper_printer.c`
- **GUI Framework**: ViewDispatcher-based with multiple views (menu, coin flip, text input)
- **Hardware**: UART communication on GPIO PA7 (9600 baud) for thermal printer
- **Stack Size**: 2KB (defined in `application.fam`)

### Core Components
```
FlipperPrinterApp (Main Context)
├── ViewDispatcher (Navigation Controller)
├── Views (UI Components)
│   ├── Submenu (menu.c) - Main menu navigation
│   ├── View (coin_flip.c) - Coin flip game interface
│   └── TextInput - Custom text input for printing
├── Printer Module (printer.c) - UART thermal printer communication
└── Application State - Coin flip statistics and text buffer
```

### Key APIs Used
- `furi_*`: Core OS primitives (memory, logging, timing)
- `gui_*`: GUI system (ViewDispatcher, Canvas, Views)
- `view_dispatcher_*`: Navigation and view management
- `notification_*`: Haptic feedback, sound, LED control
- `furi_hal_serial_*`: UART communication for printer
- `furi_hal_gpio_*`: GPIO configuration

### Resource Management Pattern
```c
// Standard Flipper allocation pattern
app = calloc(1, sizeof(FlipperPrinterApp));
app->gui = furi_record_open(RECORD_GUI);
app->view_dispatcher = view_dispatcher_alloc();
app->notification = furi_record_open(RECORD_NOTIFICATION);

// Usage phase
view_dispatcher_run(app->view_dispatcher);

// Cleanup in reverse order
view_dispatcher_free(app->view_dispatcher);
furi_record_close(RECORD_NOTIFICATION);
furi_record_close(RECORD_GUI);
free(app);
```

## Hardware Integration

### Thermal Printer (printer.c)
- **Protocol**: ESC/POS command set
- **Connection**: UART on GPIO PA7, 9600 baud
- **Features**: Text formatting (bold, size, alignment), custom headers/footers
- **Functions**: `printer_print_text()`, `printer_print_coin_flip()`

### GPIO Configuration
```c
furi_hal_gpio_init_ex(&gpio_ext_pa7, GpioModeAltFunctionPushPull, 
                      GpioPullNo, GpioSpeedVeryHigh, GpioAltFn8LPUART1);
```

## Application State Management

### Main Application Structure
```c
typedef struct {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notification;
    Submenu* menu;
    View* coin_flip_view;
    TextInput* text_input;
    char text_buffer[TEXT_BUFFER_SIZE];
    
    // Coin flip game state
    int32_t total_flips;
    int32_t heads_count;
    int32_t tails_count;
    bool last_flip_was_heads;
} FlipperPrinterApp;
```

### View Navigation
- **ViewIdMenu**: Main menu (submenu)
- **ViewIdCoinFlip**: Coin flip game interface
- **ViewIdTextInput**: Custom text input for printing

## Development Notes

1. **Build System**: Flexible Makefile supports both FBT and uFBT build tools
2. **FAP Configuration**: `application.fam` contains app metadata, source files, and build settings
3. **View Pattern**: Each view has draw, input, enter/exit callbacks following Flipper conventions
4. **Error Handling**: Comprehensive null pointer checks and safe resource management
5. **Printer Timing**: 50ms delays between printer commands for proper processing
6. **Random Generation**: Uses `furi_get_tick() % 2` for coin flip randomization