#include "settings_scene.h"
#include "../../crypto/nfc_login_passcode.h"
#include "../../hid/nfc_login_hid.h"
#include <gui/canvas.h>
#include <gui/view.h>
#include <gui/modules/widget.h>

#define PASSCODE_DOT_SIZE 6
#define PASSCODE_DOT_SPACING 10
#define PASSCODE_DOT_Y 24
#define PASSCODE_DOT_START_X 8

static void widget_custom_draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    if(!app) return;
    
    // Only draw circles if we're in passcode mode
    if(app->widget_state == 6 || app->widget_state == 7) {
        bool is_lockscreen = (app->widget_state == 7);
        size_t button_count = count_buttons_in_sequence(app->passcode_sequence);
        size_t max_buttons = MAX_PASSCODE_BUTTONS;
        
        if(is_lockscreen) {
            // Get stored sequence to know how many buttons to expect
            char stored_sequence[MAX_PASSCODE_SEQUENCE_LEN];
            if(get_passcode_sequence(stored_sequence, sizeof(stored_sequence))) {
                max_buttons = count_buttons_in_sequence(stored_sequence);
            }
        }
        
        // Draw circles
        int start_x = PASSCODE_DOT_START_X;
        for(size_t i = 0; i < max_buttons; i++) {
            int x = start_x + (i * PASSCODE_DOT_SPACING);
            int y = PASSCODE_DOT_Y;
            
            if(i < button_count) {
                // Filled circle for entered buttons
                canvas_draw_disc(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            } else {
                // Empty circle for remaining buttons
                canvas_draw_circle(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            }
        }
    }
}

void app_render_credits(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Credits");

    if(app->credits_page == 0) {
        widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, "NFC Login");
        widget_add_string_element(app->widget, 0, 22, AlignLeft, AlignTop, FontSecondary, "Version: 1.0");
        widget_add_string_element(app->widget, 0, 32, AlignLeft, AlignTop, FontSecondary, "Creator: Play2BReal");
        widget_add_string_element(app->widget, 0, 42, AlignLeft, AlignTop, FontSecondary, "github.com/Play2BReal");
    } else if(app->credits_page == 1) {
        widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, "Special Thanks To:");
        widget_add_string_element(app->widget, 0, 22, AlignLeft, AlignTop, FontSecondary, "Equip, Tac0s, WillyJL, pr3");
        widget_add_string_element(app->widget, 0, 32, AlignLeft, AlignTop, FontSecondary, "& The Biohacking Community!");
        widget_add_string_element(app->widget, 0, 42, AlignLeft, AlignTop, FontSecondary, "");
    }

    char page_info[32];
    snprintf(page_info, sizeof(page_info), "Page %d/%d  <- ->=Navigate", app->credits_page + 1, CREDITS_PAGES);
    widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, page_info);
}

void app_render_passcode_dots(App* app, size_t max_buttons) {
    // Circles are drawn by the custom draw callback
    // Just ensure the widget view has the callback set
    (void)max_buttons; // Parameter kept for API compatibility
    View* widget_view = widget_get_view(app->widget);
    if(widget_view) {
        view_set_draw_callback(widget_view, widget_custom_draw_callback);
    }
}

void app_render_lockscreen(App* app) {
    // Get the stored sequence to know how many buttons to expect
    char stored_sequence[MAX_PASSCODE_SEQUENCE_LEN];
    bool has_stored = get_passcode_sequence(stored_sequence, sizeof(stored_sequence));
    
    if(!has_stored) {
        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Error: No stored sequence");
        return;
    }
    
    // Count buttons in stored sequence
    size_t stored_button_count = count_buttons_in_sequence(stored_sequence);
    
    // Count buttons in current input
    size_t input_button_count = count_buttons_in_sequence(app->passcode_sequence);
    
    // Circles are drawn by the custom draw callback
    // Just ensure the widget view has the callback set
    View* widget_view = widget_get_view(app->widget);
    if(widget_view) {
        view_set_draw_callback(widget_view, widget_custom_draw_callback);
    }
    
    // Show progress
    char progress[32];
    snprintf(progress, sizeof(progress), "%zu / %zu", input_button_count, stored_button_count);
    
    widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, progress);
    
    if(input_button_count >= stored_button_count) {
        widget_add_string_element(app->widget, 0, 48, AlignLeft, AlignTop, FontSecondary, "Press OK to verify");
    } else {
        widget_add_string_element(app->widget, 0, 48, AlignLeft, AlignTop, FontSecondary, "Enter passcode...");
    }
}

void app_render_settings(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Settings");

    char setting_lines[SETTINGS_MENU_ITEMS][64];
    char layout_display[32];
    strncpy(layout_display, app->keyboard_layout, sizeof(layout_display) - 1);
    layout_display[sizeof(layout_display) - 1] = '\0';

    snprintf(setting_lines[0], sizeof(setting_lines[0]), "%s HID Mode: %s",
             (app->settings_menu_index == 0) ? ">" : " ", app->hid_mode == HidModeBle ? "BLE" : "USB");
    snprintf(setting_lines[1], sizeof(setting_lines[1]), "%s Keyboard Layout: %s",
             (app->settings_menu_index == 1) ? ">" : " ", layout_display);
    snprintf(setting_lines[2], sizeof(setting_lines[2]), "%s Input Delay: %dms",
             (app->settings_menu_index == 2) ? ">" : " ", app->input_delay_ms);
    snprintf(setting_lines[3], sizeof(setting_lines[3]), "%s Append Enter: %s",
             (app->settings_menu_index == 3) ? ">" : " ", app->append_enter ? "ON" : "OFF");
    snprintf(setting_lines[4], sizeof(setting_lines[4]), "%s Reset Passcode",
             (app->settings_menu_index == 4) ? ">" : " ");
    snprintf(setting_lines[5], sizeof(setting_lines[5]), "%s Disable Passcode: %s",
             (app->settings_menu_index == 5) ? ">" : " ", get_passcode_disabled() ? "ON" : "OFF");
    snprintf(setting_lines[6], sizeof(setting_lines[6]), "%s Credits",
             (app->settings_menu_index == 6) ? ">" : " ");

    for(uint8_t i = 0; i < SETTINGS_VISIBLE_ITEMS; i++) {
        uint8_t item_index = app->settings_scroll_offset + i;
        if(item_index < SETTINGS_MENU_ITEMS) {
            widget_add_string_element(app->widget, 0, 12 + i * 12, AlignLeft, AlignTop, FontSecondary, setting_lines[item_index]);
        }
    }

    if(app->settings_menu_index == 0) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "<-> Cycle  Back=Menu");
    } else if(app->settings_menu_index == 1) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "<-> Cycle  OK=Sel  Back=Menu");
    } else if(app->settings_menu_index == 2) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "<-> Cycle  Back=Menu");
    } else if(app->settings_menu_index == 3) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "OK=Toggle  Back=Menu");
    } else if(app->settings_menu_index == 4) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "OK=Reset  Back=Menu");
    } else if(app->settings_menu_index == 5) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "<-> Toggle  Back=Menu");
    } else if(app->settings_menu_index == 6) {
        widget_add_string_element(app->widget, 0, SETTINGS_HELP_Y_POS, AlignLeft, AlignTop, FontSecondary, "OK=View  Back=Menu");
    }
}