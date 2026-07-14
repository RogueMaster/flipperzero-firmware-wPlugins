/**
 * @file miband_nfc_scene_about.c
 * @brief About/Help information scene
 * 
 * This scene displays comprehensive documentation about the application,
 * including features, operations, troubleshooting tips, and usage instructions.
 * Uses a scrollable TextBox to display all information.
 */

#include "miband_nfc_i.h"

/**
 * @brief Scene entry point
 * 
 * Sets up the TextBox with comprehensive help text and displays it.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_about_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    // Set comprehensive help text
    text_box_set_text(
        app->text_box,
        "Mi Band NFC Writer v1.0\n"
        "========================\n\n"

        "FEATURES:\n"
        "- Load NFC-A dumps from /nfc folder\n"
        "- Emulate Magic Card template for Mi Band\n"
        "- Write original data to Mi Band NFC\n"
        "- Verify written data integrity\n"
        "- Save dumps with magic keys (0xFF)\n\n"

        "OPERATIONS:\n\n"

        "1. EMULATE MAGIC CARD:\n"
        "   Creates a blank template with:\n"
        "   - Original UID preserved\n"
        "   - All data blocks zeroed\n"
        "   - All keys set to FF FF FF FF FF FF\n"
        "   - Magic card access bits\n"
        "   Use this to prepare Mi Band for writing\n\n"

        "2. WRITE ORIGINAL DATA:\n"
        "   Writes actual dump data to Mi Band:\n"
        "   - Tries dump keys first\n"
        "   - Falls back to 0xFF keys if needed\n"
        "   - Preserves UID block (read-only)\n"
        "   - Includes all data + sector trailers\n"
        "   - Real-time progress feedback\n\n"

        "3. SAVE MAGIC DUMP:\n"
        "   Converts dump for magic card use:\n"
        "   - Changes all keys to 0xFF\n"
        "   - Sets magic access bits\n"
        "   - Saves with '_magic' suffix\n"
        "   - Compatible with magic card writers\n\n"

        "4. VERIFY WRITE:\n"
        "   Checks if write was successful:\n"
        "   - Reads Mi Band with dump keys\n"
        "   - Compares all data blocks\n"
        "   - Reports differences found\n\n"

        "TROUBLESHOOTING:\n\n"

        "Write Fails:\n"
        "- Ensure card is magic card type\n"
        "- Check card positioning\n"
        "- Try emulate magic template first\n\n"

        "USAGE TIPS:\n"
        "- Always backup original dumps\n"
        "- Position cards carefully\n"
        "- Use verify after writing\n\n"

        "For support and updates:\n"
        "Check Flipper Zero community forums\n\n"

        "Press Back to return to main menu.");

    // Use text font for better readability
    text_box_set_font(app->text_box, TextBoxFontText);

    // Start at the beginning of the text
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    // Switch to text box view
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdAbout);
}

/**
 * @brief Scene event handler
 * 
 * Handles back button press to return to main menu.
 * 
 * @param context Pointer to MiBandNfcApp instance
 * @param event Scene manager event
 * @return true if event was consumed, false otherwise
 */
bool miband_nfc_scene_about_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // User pressed back - return to main menu
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Scene exit handler
 * 
 * Resets the TextBox when leaving the scene.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_about_on_exit(void* context) {
    MiBandNfcApp* app = context;
    text_box_reset(app->text_box);
}
