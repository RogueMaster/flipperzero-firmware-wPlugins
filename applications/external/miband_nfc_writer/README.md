# Mi Band NFC Writer v1.0

A comprehensive Flipper Zero application for managing NFC data on Xiaomi Mi Band devices with advanced features including automatic backups, detailed logging, and progress tracking.

## Overview

This application provides a complete toolkit for reading, writing, emulating, and verifying NFC data on Mi Band smart bands. It supports the full workflow from preparing a blank Mi Band to writing custom NFC dumps with real-time progress feedback and automatic verification.

## Features

### Core Operations
- **Quick UID Check**: Instantly read card UID and scan disk for matching dumps
- **Load NFC Dumps**: Browse and load Mifare Classic dumps from `/ext/nfc` folder
- **Magic Card Emulation**: Create blank template with 0xFF keys for Mi Band initialization
- **Write Original Data**: Write dump data to Mi Band with intelligent key detection
- **Automatic Backup**: Optional pre-write backup with timestamp
- **Verify Write**: Read and compare written data with original dump
- **Difference Viewer**: Detailed hex comparison of mismatched blocks
- **Save Magic Dumps**: Convert dumps to magic format with 0xFF keys

### Advanced Features
- **Settings System**: Persistent configuration storage
  - Auto-backup before write
  - Automatic verification after write
  - Detailed progress display
  - Enable/disable logging
- **Logging System**: Complete operation logging with export capability
  - Timestamped log entries
  - Exportable to text files
  - Up to 500 entries stored
  - Debug, Info, Warning, Error levels
- **Progress Tracking**: Real-time feedback with ASCII progress bars
  - Sector-by-sector progress
  - Percentage completion
  - Authentication statistics
  - Operation status messages

## Architecture

### File Structure

```
miband_nfc_writer/
├── miband_nfc.c                      # Main entry point and lifecycle
├── miband_nfc.h                      # Public header (opaque type)
├── miband_nfc_i.h                    # Internal header (structures)
├── miband_logger.c                   # Logging system implementation
├── miband_logger.h                   # Logging system interface
├── progress_tracker.c                # Progress tracking utilities
├── progress_tracker.h                # Progress tracker interface
├── miband_nfc_scene.c                # Scene handler arrays
├── miband_nfc_scene.h                # Scene declarations
├── miband_nfc_scene_config.h         # Scene configuration (X-Macro)
├── miband_nfc_scene_main_menu.c      # Main menu
├── miband_nfc_scene_file_select.c    # File browser
├── miband_nfc_scene_magic_emulator.c # Magic card emulation with stats
├── miband_nfc_scene_writer.c         # Data writing with progress
├── miband_nfc_scene_backup.c         # Automatic backup system
├── miband_nfc_scene_verify.c         # Data verification
├── miband_nfc_scene_diff_viewer.c    # Detailed difference display
├── miband_nfc_scene_magic_saver.c    # Magic dump converter
├── miband_nfc_scene_uid_check.c      # Quick UID reader and scanner
├── miband_nfc_scene_settings.c       # Settings configuration
├── miband_nfc_scene_about.c          # Help/documentation
└── miband_nfc_icons.c                # Icon assets
```

### Key Components

#### Scene Manager Pattern
Uses Flipper's scene manager with X-Macro pattern for scalable scene handling:
- Each scene has three handlers: `on_enter`, `on_event`, `on_exit`
- Scenes defined once in `miband_nfc_scene_config.h`
- Automatic generation of handler arrays and enumerations

#### NFC Operations
- **Scanner**: Detects and identifies NFC cards
- **Poller**: Reads data from physical cards
- **Listener**: Emulates NFC cards with real-time statistics
- Uses synchronous polling API for reliable read/write operations

#### Logging System
- Circular buffer with 500 entry capacity
- Automatic log rotation (keeps newest 75% when full)
- Timestamped entries with log levels
- Export to `/ext/apps_data/miband_nfc/logs/`
- Enable/disable via settings

#### Settings System
- Persistent storage in `/ext/apps_data/miband_nfc/settings.bin`
- Magic number validation
- Auto-load on startup
- Configurable options:
  - Auto backup enabled
  - Verify after write
  - Show detailed progress
  - Enable logging

## Operations

### 0. Quick UID Check

**Purpose**: Rapidly identify a card and find matching dumps on disk.

**Process**:
1. Reads Block 0 from placed card
2. Displays UID, BCC, SAK, ATQA
3. Scans `/ext/nfc` directory (non-recursive)
4. Lists all matching dump files
5. Shows manufacturer data

**Use Case**: Quick identification before operations, find which dumps match your Mi Band.

### 1. Emulate Magic Card

**Purpose**: Prepare a new Mi Band for writing by emulating a blank template.

**Process**:
1. Loads selected dump file
2. Preserves original UID from dump
3. Zeros all data blocks
4. Sets all keys to `FF FF FF FF FF FF`
5. Configures magic access bits
6. Emulates the template via NFC
7. Shows real-time statistics:
   - Authentication attempts and successes
   - Connection count
   - Last activity status

**Use Case**: Use this BEFORE first write to initialize a blank Mi Band with magic keys.

**UI Features**:
- Real-time auth counter
- Success/fail statistics
- Status messages
- Animated dolphin icon
- Back button to stop

### 2. Write Original Data

**Purpose**: Write actual dump data to Mi Band NFC chip.

**Process**:
1. Optional automatic backup (if enabled in settings)
2. Detects card and determines current key state
3. Preserves Block 0 UID (read-only)
4. Writes all data blocks (1-15 for each sector)
5. Writes sector trailers with keys and access bits
6. Uses intelligent authentication:
   - First write scenario: tries 0xFF keys first
   - Rewrite scenario: tries dump keys first
7. Real-time progress display:
   - ASCII progress bar
   - Percentage completion
   - Current sector number
8. Optional automatic verification (if enabled)

**Key Features**:
- Automatic pre-write backup with timestamp
- Automatic key detection (magic vs original)
- Block 0 UID preservation
- Retry logic for reliability
- Progress bar updates every sector
- Logged to system if enabled

### 3. Save Magic Dump

**Purpose**: Convert dump file to magic format for easy handling.

**Process**:
1. Loads original dump
2. Shows conversion progress per sector
3. Modifies all sector trailers:
   - Key A: `FF FF FF FF FF FF`
   - Key B: `FF FF FF FF FF FF`
   - Access bits: `FF 07 80 69`
4. Saves with `_magic.nfc` suffix
5. Progress bar during conversion

**Use Case**: Create magic-compatible versions of dumps for testing or simplified writing.

### 4. Verify Write

**Purpose**: Confirm data was written correctly to Mi Band.

**Process**:
1. Reads all sectors from Mi Band
2. Shows detailed progress:
   - Sector-by-sector reading
   - Authentication statistics
   - Progress bar and percentage
3. Tries dump keys first, falls back to 0xFF keys
4. Compares data intelligently:
   - Block 0: **SKIPPED** (UID is preserved, not compared)
   - Sector trailers: **SKIPPED** (keys are expected to differ)
   - Data blocks: Full comparison (16 bytes)
5. Reports differences if found
6. Can show detailed difference viewer

**Key Fix**: The verify operation now:
- Skips Block 0 completely (UID preservation)
- Skips all sector trailers (key differences expected)
- Only compares actual data blocks
- Handles both key scenarios gracefully

### 5. Difference Viewer

**Purpose**: Show detailed hex comparison of mismatched blocks.

**Features**:
- Lists all differing blocks
- Shows expected vs found data in hex
- Highlights different bytes with `[XX]` notation
- Displays block types (Data/Trailer/UID)
- Shows byte-level differences
- Scrollable text view

### 6. Automatic Backup

**Purpose**: Create timestamped backups before write operations.

**Process**:
1. Triggered automatically if enabled in settings
2. Reads current Mi Band data
3. Creates backup in `/ext/nfc/backups/`
4. Filename: `backup_YYYYMMDD_HHMMSS.nfc`
5. Shows progress during backup
6. Proceeds to write after successful backup

**Safety**: Ensures you can always restore original data.

### 7. Settings

**Configurable Options**:
- **Auto Backup**: Create backup before each write
- **Verify After Write**: Automatically verify after writing
- **Detailed Progress**: Show progress bars and percentages
- **Enable Logging**: Log all operations to file
- **Export Logs**: Export current logs with timestamp
- **Clear Logs**: Reset log buffer

**Persistence**: Settings saved to persistent storage and loaded on app start.

## Technical Details

### Authentication Strategy

The writer implements a three-tier authentication strategy:

```c
// Priority order for authentication:
1. Try 0xFF keys (if pre-check indicates magic state)
2. Try dump Key A
3. Try dump Key B
4. Try 0xFF keys as last resort (if not already tried)
```

### Block 0 Handling

Block 0 is special because it contains the UID which is typically read-only:

```c
// Block 0 structure:
// [0-3]   UID (4 bytes)
// [4]     BCC (1 byte) = UID[0] ^ UID[1] ^ UID[2] ^ UID[3]
// [5]     SAK
// [6-7]   ATQA
// [8-15]  Manufacturer data
```

The writer:
1. Reads current Block 0
2. Preserves existing UID
3. Writes only blocks 1-2 in sector 0
4. Writes sector 0 trailer
5. **NEVER compares Block 0 during verification**

### Sector Trailer Format

Magic card format:
```
Bytes 0-5:   Key A = FF FF FF FF FF FF
Bytes 6-9:   Access bits = FF 07 80 69
Bytes 10-15: Key B = FF FF FF FF FF FF
```

These access bits allow read/write with any key, perfect for magic cards.

### Verification Comparison Logic

```c
if (block_index == 0) {
    // SKIP - UID block is preserved, not compared
    continue;
} else if (is_sector_trailer(block_index)) {
    // SKIP - keys are expected to differ
    continue;
} else {
    // Full comparison for data blocks only
    compare(original[0:16], read[0:16]);
}
```

This ensures only actual data is compared, avoiding false mismatches from UID preservation or key changes.

### Progress Display

All long operations show real-time progress:
```
Writing sector 12/16

[============>       ]
75%
```

ASCII progress bar with:
- Current/total items
- Visual progress bar (20 characters)
- Percentage completion

### Logging System

```c
// Log levels
LogLevelDebug   - Detailed debugging information
LogLevelInfo    - General information messages
LogLevelWarning - Warning conditions
LogLevelError   - Error conditions

// Log format
YYYY-MM-DD HH:MM:SS [LEVEL] Message

// Example
2024-12-27 14:32:15 [INFO] Write started: /ext/nfc/miband.nfc
2024-12-27 14:32:45 [INFO] Write completed successfully
```

Logs are:
- Circular buffer (500 entries)
- Auto-rotating when full
- Exportable with timestamp
- Enable/disable in settings

## Troubleshooting

### Write Fails

**Problem**: "All authentication methods FAILED"

**Solutions**:
1. Ensure Mi Band is a magic card type
2. Check card positioning (hold steady)
3. Run "Emulate Magic Card" first
4. Verify dump file is valid Mifare Classic
5. Check logs if enabled

### Verify Shows Differences

**Problem**: "Data mismatch found"

**Check Logs/Difference Viewer**:
- Use difference viewer to see exactly what differs
- Check if only specific sectors differ
- Verify original dump is correct
- Try rewriting the affected sectors

**Note**: Verify now skips Block 0 and trailers, so any reported differences are real data mismatches.

### Backup Fails

**Problem**: "Cannot create backup"

**Solutions**:
1. Check SD card has free space
2. Ensure `/ext/nfc/backups/` directory exists
3. Verify card can be read
4. Check file permissions

### Emulation Issues

**Problem**: "Too many auth attempts" (stops at 100)

**Cause**: Reader continuously authenticating without reading

**Solution**: 
1. Check that dump file is valid
2. Verify UID is correct
3. Try with different dump file
4. Check emulation statistics for patterns

### Settings Not Saving

**Problem**: Settings reset after closing app

**Solutions**:
1. Check `/ext/apps_data/miband_nfc/` exists
2. Verify SD card is not write-protected
3. Check logs for save errors
4. Reinstall app if structure corrupted

## Usage Workflow

### First-Time Write to Blank Mi Band

1. **Prepare**: Load your dump file
   - Main Menu → "Write Original Data" → Select file
2. **Initialize**: Emulate magic template
   - Main Menu → "Emulate Magic Card"
   - Place Mi Band near Flipper
   - Watch auth counter reach ~16-32
   - Remove Mi Band, press Back
3. **Write**: Write actual data
   - Main Menu → "Write Original Data"
   - (Automatic backup if enabled)
   - Place Mi Band near Flipper
   - Watch progress bar complete
   - Wait for "Success" message
4. **Verify**: Confirm write
   - (Automatic if enabled in settings)
   - Or manually: "Verify Write"
   - Check for any differences

### Rewriting Mi Band (Data Already Present)

1. Choose "Write Original Data"
2. Select dump file
3. Auto-backup runs (if enabled)
4. Place Mi Band near Flipper
5. Write automatically detects existing keys
6. Auto-verify runs (if enabled)
7. Check results

### Quick Card Identification

1. Choose "Quick UID Check"
2. Place any NFC card near Flipper
3. View UID and card information
4. See list of matching dumps on SD card
5. Press Back to return

### Creating Magic Dumps

1. Choose "Save Magic Dump"
2. Select source dump file
3. Watch conversion progress
4. New file created with `_magic.nfc` suffix
5. Use this file for easier magic card operations

### Managing Logs

1. Settings → "Enable Logging" (ON)
2. Perform operations
3. Settings → "Export Logs"
4. Find in `/ext/apps_data/miband_nfc/logs/log_YYYYMMDD_HHMMSS.txt`
5. Settings → "Clear Logs" to reset

## File Locations

```
/ext/nfc/                          - NFC dump files
/ext/nfc/backups/                  - Automatic backups
/ext/apps_data/miband_nfc/         - App data directory
/ext/apps_data/miband_nfc/logs/    - Exported log files
/ext/apps_data/miband_nfc/settings.bin - Settings file
```

## Development Notes

### Adding New Scenes

1. Add scene entry to `miband_nfc_scene_config.h`:
   ```c
   ADD_SCENE(miband_nfc, my_new_scene, MyNewScene)
   ```

2. Create `miband_nfc_scene_my_new_scene.c` with handlers:
   ```c
   void miband_nfc_scene_my_new_scene_on_enter(void* context);
   bool miband_nfc_scene_my_new_scene_on_event(void* context, SceneManagerEvent event);
   void miband_nfc_scene_my_new_scene_on_exit(void* context);
   ```

3. Handlers are automatically registered via X-Macro pattern

### Progress Display Guidelines

For long operations, update UI frequently:
```c
// Update text every iteration
popup_set_text(app->popup, progress_text, 64, 18, AlignCenter, AlignTop);

// Use delays to allow GUI updates
furi_delay_ms(100);

// Don't call popup_reset() during progress
// Set header/icon once at start, update text only
```

### Memory Management

- All allocations in `miband_nfc_app_alloc()`
- All deallocations in `miband_nfc_app_free()`
- Scene-specific resources allocated in `on_enter`
- Scene-specific resources freed in `on_exit`
- Logger allocated before settings load

### Logging Best Practices

```c
// Log important operations
miband_logger_log(app->logger, LogLevelInfo, "Operation started");

// Log errors with details
miband_logger_log(app->logger, LogLevelError, "Failed: sector %zu", sector);

// Check if logging enabled before expensive operations
if(app->enable_logging) {
    // Prepare detailed message
}
```

## Performance Notes

- Progress updates every 50-100ms for smooth display
- Maximum 20 tool calls for file operations
- Circular log buffer prevents memory exhaustion
- Non-recursive directory scanning for speed
- Settings cached in memory, saved on change

## Credits

Developed for Flipper Zero by LucasMac.

Based on official Flipper NFC APIs and extensive testing with Mi Band devices.

## License

Check Flipper Zero application license requirements.

---

**Version**: 1.0  
**Last Updated**: 20251002  
**Compatibility**: Flipper Zero Official Firmware