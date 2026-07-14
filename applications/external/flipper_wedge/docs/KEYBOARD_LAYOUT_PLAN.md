# Keyboard Layout Support - Implementation Plan

## Overview

Add configurable keyboard layout support to Flipper Wedge, allowing users to output UIDs correctly on non-US keyboard layouts (Hungarian, AZERTY, Dvorak, etc.).

## Design Decisions

- **File format**: FlipperFormat (.txt) - matches Flipper's native format
- **Fallback**: Unmapped characters use firmware default (US QWERTY)
- **Built-in layouts**: "Default (QWERTY)" + "NumPad" hardcoded options

---

## Layout File Format

**Location**: `/ext/apps_data/flipper_wedge/layouts/`

**File structure**:
```
Filetype: Flipper Wedge Keyboard Layout
Version: 1
Name: Hungarian

# Format: <char>: <hid_keycode_decimal> [SHIFT]
# Only characters used in UID output need mapping:
# - Digits 0-9
# - Hex letters A-F, a-f
# - Delimiters: space, dash, colon, comma

# Digits (Hungarian: 0 is left of 1)
0: 39
1: 30
2: 31
3: 32
4: 33
5: 34
6: 35
7: 36
8: 37
9: 38

# Uppercase hex (with SHIFT modifier)
A: 4 SHIFT
B: 5 SHIFT
C: 6 SHIFT
D: 7 SHIFT
E: 8 SHIFT
F: 9 SHIFT

# Lowercase hex
a: 4
b: 5
c: 6
d: 7
e: 8
f: 9

# Common delimiters (adjust per layout)
-: 45
:: 51 SHIFT
```

**Notes**:
- Single printable ASCII character as key (before colon)
- HID keycode in decimal (matching USB HID spec)
- Optional `SHIFT` modifier suffix
- Lines starting with `#` are comments
- Unmapped characters fall back to firmware default

---

## Built-in Layouts

### 1. Default (QWERTY)
- Uses firmware's `HID_ASCII_TO_KEY()` macro directly
- No custom mapping, standard US QWERTY behavior

### 2. NumPad
- Hardcoded mapping for digits and hex letters to numpad keycodes:
  - `0-9` → `HID_KEYPAD_0` (0x62), `HID_KEYPAD_1-9` (0x59-0x61)
  - `A-F`, `a-f` → `HID_KEYPAD_A-F` (0xBC-0xC1)
- Works on any keyboard layout (numpad is layout-independent)
- Requires NumLock active on host

---

## Data Structures

```c
// helpers/flipper_wedge_keyboard_layout.h

#define FLIPPER_WEDGE_LAYOUT_NAME_MAX 32
#define FLIPPER_WEDGE_LAYOUT_PATH_MAX 128

// Built-in layout identifiers
typedef enum {
    FlipperWedgeLayoutDefault,    // Use firmware HID_ASCII_TO_KEY
    FlipperWedgeLayoutNumPad,     // Use numpad keycodes
    FlipperWedgeLayoutCustom,     // Load from file
    FlipperWedgeLayoutCount,
} FlipperWedgeLayoutType;

// Single character mapping
typedef struct {
    uint16_t keycode;    // HID keycode (lower 8 bits) + modifiers (upper 8 bits)
    bool defined;        // true if explicitly defined in layout
} FlipperWedgeKeyMapping;

// Complete keyboard layout
typedef struct {
    char name[FLIPPER_WEDGE_LAYOUT_NAME_MAX];
    char file_path[FLIPPER_WEDGE_LAYOUT_PATH_MAX];  // Empty for built-in
    FlipperWedgeLayoutType type;
    FlipperWedgeKeyMapping map[128];  // ASCII 0-127
} FlipperWedgeKeyboardLayout;
```

---

## File Changes

### 1. New files

**helpers/flipper_wedge_keyboard_layout.h**
```c
#pragma once

#include <furi.h>
#include <storage/storage.h>

// ... structs from above ...

// Allocate/free layout
FlipperWedgeKeyboardLayout* flipper_wedge_keyboard_layout_alloc(void);
void flipper_wedge_keyboard_layout_free(FlipperWedgeKeyboardLayout* layout);

// Set built-in layout
void flipper_wedge_keyboard_layout_set_default(FlipperWedgeKeyboardLayout* layout);
void flipper_wedge_keyboard_layout_set_numpad(FlipperWedgeKeyboardLayout* layout);

// Load custom layout from file
bool flipper_wedge_keyboard_layout_load(FlipperWedgeKeyboardLayout* layout, const char* path);

// Get keycode for character (with fallback to firmware default)
uint16_t flipper_wedge_keyboard_layout_get_keycode(FlipperWedgeKeyboardLayout* layout, char c);

// List available layout files on SD card
// Returns FuriString* array, caller must free
size_t flipper_wedge_keyboard_layout_list(Storage* storage, FuriString** names, FuriString** paths, size_t max_count);
```

**helpers/flipper_wedge_keyboard_layout.c**
- Implementation of above functions
- NumPad mapping table (hardcoded)
- FlipperFormat parser for custom layouts

### 2. Modified files

**flipper_wedge.h**
```c
// Add to FlipperWedge struct:
FlipperWedgeKeyboardLayout* keyboard_layout;
FlipperWedgeLayoutType layout_type;
char layout_file_path[FLIPPER_WEDGE_LAYOUT_PATH_MAX];
```

**helpers/flipper_wedge_storage.h**
```c
// Add settings key
#define FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_TYPE "LayoutType"
#define FLIPPER_WEDGE_SETTINGS_KEY_LAYOUT_FILE "LayoutFile"
```

**helpers/flipper_wedge_storage.c**
- Save/load `layout_type` and `layout_file_path`
- Increment `FLIPPER_WEDGE_SETTINGS_FILE_VERSION` to 6

**helpers/flipper_wedge_hid.c**
```c
// Modify flipper_wedge_hid_type_char():
void flipper_wedge_hid_type_char(FlipperWedgeHid* instance, FlipperWedgeKeyboardLayout* layout, char c) {
    uint16_t keycode = flipper_wedge_keyboard_layout_get_keycode(layout, c);
    // ... rest unchanged
}
```

**helpers/flipper_wedge_hid.h**
- Update function signature to accept layout parameter

**flipper_wedge.c**
- Allocate/free keyboard_layout in app init/deinit
- Load layout based on settings
- Pass layout to HID typing functions

**scenes/flipper_wedge_scene_settings.c**
- Add "Keyboard Layout" item with submenu:
  - "Default (QWERTY)"
  - "NumPad"
  - "Custom..." → file browser for /layouts/ folder

---

## Sample Layout Files

Create in `assets/layouts/` (to be copied to SD):

1. **hungarian.txt** - Hungarian keyboard
2. **azerty_fr.txt** - French AZERTY
3. **dvorak.txt** - Dvorak layout

---

## Implementation Order

1. Create `flipper_wedge_keyboard_layout.c/h` with data structures and core functions
2. Implement NumPad hardcoded layout
3. Implement FlipperFormat parser for custom layouts
4. Update `flipper_wedge.h` with new fields
5. Update storage to save/load layout settings
6. Modify HID module to use layout for typing
7. Add settings UI for layout selection
8. Create sample layout files
9. Test on multiple firmwares

---

## Testing Checklist

- [ ] Default (QWERTY) outputs correctly on US keyboard
- [ ] NumPad mode outputs correct digits (with NumLock on)
- [ ] Custom layout loads from SD card
- [ ] Invalid layout file handled gracefully (falls back to default)
- [ ] Missing layout file at startup handled (falls back to default)
- [ ] Layout setting persists across restarts
- [ ] All 4 firmwares build without warnings
- [ ] No memory leaks (layout alloc/free)

---

## Future Enhancements (Out of Scope)

- Layout editor on device
- Auto-detect host keyboard layout
- More modifier support (Ctrl, Alt, AltGr)
- Unicode/extended ASCII support
