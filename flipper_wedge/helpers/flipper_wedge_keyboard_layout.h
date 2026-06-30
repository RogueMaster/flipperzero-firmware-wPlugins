#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <furi_hal_usb_hid.h>

#define FLIPPER_WEDGE_LAYOUT_NAME_MAX 32
#define FLIPPER_WEDGE_LAYOUT_PATH_MAX 128
#define FLIPPER_WEDGE_LAYOUTS_DIRECTORY EXT_PATH("apps_data/flipper_wedge/layouts")

// Built-in layout identifiers
typedef enum {
    FlipperWedgeLayoutDefault,  // Use firmware HID_ASCII_TO_KEY
    FlipperWedgeLayoutNumPad,   // Use numpad keycodes for 0-9, A-F
    FlipperWedgeLayoutCustom,   // Load from file
    FlipperWedgeLayoutCount,
} FlipperWedgeLayoutType;

// Single character mapping
typedef struct {
    uint16_t keycode;  // HID keycode (lower 8 bits) + modifiers (upper 8 bits)
    bool defined;      // true if explicitly defined in layout
} FlipperWedgeKeyMapping;

// Complete keyboard layout
typedef struct {
    char name[FLIPPER_WEDGE_LAYOUT_NAME_MAX];
    char file_path[FLIPPER_WEDGE_LAYOUT_PATH_MAX];
    FlipperWedgeLayoutType type;
    FlipperWedgeKeyMapping map[128];  // ASCII 0-127
} FlipperWedgeKeyboardLayout;

/** Allocate keyboard layout
 *
 * @return FlipperWedgeKeyboardLayout instance (initialized to Default)
 */
FlipperWedgeKeyboardLayout* flipper_wedge_keyboard_layout_alloc(void);

/** Free keyboard layout
 *
 * @param layout FlipperWedgeKeyboardLayout instance
 */
void flipper_wedge_keyboard_layout_free(FlipperWedgeKeyboardLayout* layout);

/** Set layout to Default (QWERTY) - uses firmware HID_ASCII_TO_KEY
 *
 * @param layout FlipperWedgeKeyboardLayout instance
 */
void flipper_wedge_keyboard_layout_set_default(FlipperWedgeKeyboardLayout* layout);

/** Set layout to NumPad mode - uses numpad keycodes for hex digits
 *
 * @param layout FlipperWedgeKeyboardLayout instance
 */
void flipper_wedge_keyboard_layout_set_numpad(FlipperWedgeKeyboardLayout* layout);

/** Load custom layout from file
 *
 * @param layout FlipperWedgeKeyboardLayout instance
 * @param path Path to layout file
 * @return true if loaded successfully, false on error (layout unchanged)
 */
bool flipper_wedge_keyboard_layout_load(FlipperWedgeKeyboardLayout* layout, const char* path);

/** Get HID keycode for character
 *
 * @param layout FlipperWedgeKeyboardLayout instance
 * @param c ASCII character to map
 * @return HID keycode with modifiers, or HID_KEYBOARD_NONE if unmappable
 */
uint16_t flipper_wedge_keyboard_layout_get_keycode(FlipperWedgeKeyboardLayout* layout, char c);

/** Get layout type name for display
 *
 * @param type Layout type enum
 * @return Static string name
 */
const char* flipper_wedge_keyboard_layout_type_name(FlipperWedgeLayoutType type);

/** List available custom layout files on SD card
 *
 * @param storage Storage instance
 * @param names Array to fill with layout names (from file content)
 * @param paths Array to fill with file paths
 * @param max_count Maximum number of layouts to return
 * @return Number of layouts found
 */
size_t flipper_wedge_keyboard_layout_list(
    Storage* storage,
    FuriString** names,
    FuriString** paths,
    size_t max_count);
