# CLAUDE.md - VK Thermo Flipper Maintenance Guide

**Purpose**: Primary reference for AI assistants maintaining the VK Thermo Flipper application.

**Status**: v1.0 Released — Maintenance Mode

**Author**: VivoKey Technologies

---

## Quick Navigation

- **[README.md](README.md)** — User documentation
- **[docs/changelog.md](docs/changelog.md)** — Version history
- **[manifest.yml](manifest.yml)** — App catalog manifest

---

## Maintenance Philosophy

1. **Stability first** — Fix bugs, don't add features unless requested
2. **Cross-firmware compatibility** — Must build on Official, Unleashed, Momentum
3. **No `#ifdef` guards** — APIs are identical across firmwares; one codebase
4. **Keep it simple** — No over-engineering or unused code
5. **Compile clean** — Zero warnings on all firmwares

### What NOT to Do
- Don't add features without being asked
- Don't refactor working code for aesthetics
- Don't add Momentum/Unleashed-specific API calls
- Don't change NFC protocol logic without hardware testing

---

## Build & Deploy

```bash
# Build for specific firmware
./build.sh official      # Official (default: latest stable tag)
./build.sh unleashed     # Unleashed (default: release branch)
./build.sh momentum      # Momentum (default: release branch)

# Build with specific version
./build.sh official --tag 1.4.3

# Build and deploy to connected Flipper
./deploy.sh official
```

**Build Output**: `dist/vk_thermo_<version>_<firmware>_<fw-version>.fap`

**Firmware clones** (required):
- `/home/work/flipperzero-firmware` (Official)
- `/home/work/unleashed-firmware` (Unleashed)
- `/home/work/Momentum-Firmware` (Momentum)

Build script creates symlink: `<firmware>/applications_user/vk_thermo` → this project.

### Tested Firmware Versions
- Official: 1.4.3
- Unleashed: release branch
- Momentum: release branch

---

## Code Architecture

### File Structure

```
flipper-thermo/
├── application.fam              # App manifest
├── manifest.yml                 # App catalog manifest
├── vk_thermo.c                  # App entry, alloc/free, backlight enforce
├── vk_thermo.h                  # Main app struct and enums
├── build.sh / deploy.sh         # Build and deploy scripts
├── docs/
│   └── changelog.md             # Version history
├── icons/
│   └── vk_thermo_10px.png       # App icon
├── helpers/
│   ├── vk_thermo_custom_event.h # Custom event definitions
│   ├── vk_thermo_haptic.c/h     # Haptic feedback
│   ├── vk_thermo_led.c/h        # LED feedback
│   ├── vk_thermo_speaker.c/h    # Sound feedback
│   ├── vk_thermo_storage.c/h    # Settings + per-UID CSV persistence
│   └── vk_thermo_nfc.c/h        # NFC + NTAG5Link + TMP112
├── scenes/
│   ├── vk_thermo_scene.c/h      # Scene handlers
│   ├── vk_thermo_scene_config.h # Scene definitions
│   ├── vk_thermo_scene_scan.c   # Main scanning scene
│   ├── vk_thermo_scene_log.c    # History view scene
│   ├── vk_thermo_scene_graph.c  # Graph view scene
│   └── vk_thermo_scene_settings.c # Settings scene
└── views/
    ├── vk_thermo_scan_view.c/h  # Scan + temperature display
    ├── vk_thermo_log_view.c/h   # History list with UID selector
    └── vk_thermo_graph_view.c/h # Graph with Bezier curves + comparison mode
```

### Scene Flow

```
                        ┌─────────┐
              Left      │  Graph  │
           ┌──────────► │         │
           │            └─────────┘
┌─────────┐     OK      ┌─────────┐
│  Scan   │ ──────────► │   Log   │
│ (main)  │             │         │
└────┬────┘             └─────────┘
     │
     │ Right            ┌─────────┐
     └────────────────► │Settings │
     │                  └─────────┘
     │ Back
     ▼
   Exit
```

### Key Navigation

**Scan**: Left=Graph, OK=Log, Right=Settings, Back=Exit
**Log**: Left/Right=Cycle UIDs, OK long=Clear history, Up/Down=Scroll, Back=Scan
**Graph**: Left/Right/Up/Down=Cycle UIDs, OK=Toggle comparison mode, Back=Scan

---

## NFC Communication Stack

```
Flipper Zero NFC API (ISO15693)
    │
    ├── Scanner → detects tag → switches to Poller
    │
    ▼
NXP Custom Commands (manufacturer code 0x04)
    │
    ├── WRITE_CONFIG (0xC1) — Energy harvesting (two-step: TRIGGER → poll LOAD_OK → ENABLE)
    ├── READ_CONFIG (0xC0) — Check EH status, I2C busy/result
    │
    ▼
NTAG5Link I2C Passthrough
    │
    ├── WRITE_I2C (0xD4) — Set TMP112 register pointer to 0x00
    ├── READ_I2C (0xD5) — Read 2 bytes into SRAM
    ├── READ_SRAM (0xD2) — Fetch actual temperature data from SRAM
    │
    ▼
TMP112 Temperature Sensor (I2C addr 0x48)
```

### Critical: I2C Passthrough Flow

Data does NOT return in the NFC response. Must read from SRAM:
1. Check I2C busy (READ_CONFIG at addr 0xAD)
2. WRITE_I2C — set register pointer
3. Check I2C result (READ_CONFIG at 0xAD, check trans_status)
4. READ_I2C — initiates read into SRAM
5. **READ_SRAM** — fetch actual data from SRAM

### Energy Harvesting

Two-step process:
1. TRIGGER only (charge capacitor), poll LOAD_OK at 100ms intervals
2. TRIGGER + ENABLE (activate voltage output)

- WRITE_CONFIG commands may timeout but succeed — treat timeout as OK
- LOAD_OK can take several seconds on Flipper (weaker field than desktop readers)
- Timeout is user-configurable in settings (1s, 2s, 5s, 10s, 30s, indefinite)
- Cooperative cancellation via `volatile bool stop_requested` flag

### Temperature Conversion (TMP112)

```c
// TMP112: 12-bit signed value, left-justified in 16-bit register (bits 15:4)
// Resolution: 0.0625°C per LSB
int16_t temp_12bit = raw_value >> 4;
float celsius = (float)temp_12bit * 0.0625f;
```

**NOT TMP117** — same I2C address (0x48) and register (0x00), but different conversion.

### NFC Protocol Notes

- Flipper stores UIDs MSB-first; ISO15693 commands need LSB-first
- After inventory, tag is in Quiet state — use addressed mode (0x22)
- NXP manufacturer code: 0x04

---

## Data Storage

### Settings File
**Location**: `/ext/apps_data/vk_thermo/vk_thermo.conf`

| Key       | Values                  | Description              |
|-----------|-------------------------|--------------------------|
| Haptic    | 0/1                     | Vibration feedback       |
| Speaker   | 0/1                     | Sound feedback           |
| Led       | 0/1                     | LED feedback             |
| TempUnit  | 0=Celsius, 1=Fahrenheit | Display preference       |
| EhTimeout | 0-5                     | EH timeout index         |
| Debug     | 0/1                     | NFC diagnostic tests     |

### CSV Log (Per-UID Files)
**Location**: `/ext/apps_data/vk_thermo/<UID_HEX>.csv`

Each implant gets its own file named after its 16-char hex UID.

```csv
timestamp,celsius,fahrenheit
1706918400,36.52,97.74
1706918520,36.48,97.66
```

Legacy `readings.csv` (old format with uid column) is auto-migrated on first load.

### In-Memory Log
- Circular buffer: 50 entries max (`VK_THERMO_LOG_MAX_ENTRIES`)
- Stats calculated on demand: min, max per UID

---

## Backlight

- App start: `sequence_display_backlight_enforce_on` (screen stays on)
- App exit: `sequence_display_backlight_enforce_auto` (restores system timeout)

---

## Troubleshooting

### Build Fails
1. Check symlink: `ls -la <firmware>/applications_user/vk_thermo`
2. Verify firmware version compatibility
3. Check firmware changelog for API changes

### NFC Not Detecting
1. Verify ISO15693 (not ISO14443)
2. Check poller state machine in `vk_thermo_nfc.c`
3. Test with Flipper's built-in NFC Debug app

### Temperature Read Fails
1. Check energy harvesting succeeded (LOAD_OK)
2. Verify I2C passthrough flow (WRITE_I2C → READ_I2C → READ_SRAM)
3. Check TMP112 register pointer (0x00)
4. Review NXP command frame structure

### App Unresponsive on Back
- `stop_requested` flag should abort EH polling loop
- Set BEFORE `nfc_poller_stop()` (which blocks until callback returns)

---

## Quality Gates

### Before Committing
- Code compiles without warnings on Official firmware
- Change follows existing code patterns
- No unused code or dead paths

### Before Release
- Build succeeds on all three firmwares
- Core functionality tested on hardware
- CSV export works correctly
- Settings persist across restarts
- README.md and docs/changelog.md updated

---

## Reference Files

| File | Use For |
|------|---------|
| `/home/work/flipper wedge/` | Multi-firmware patterns, app store submission model |
| `/home/work/ntag5sensor/vicinity/ntag5link.py` | NXP custom commands, I2C passthrough |
| `/home/work/ntag5sensor/vicinity/tmp117.py` | TMP112 registers (file misnamed) |
