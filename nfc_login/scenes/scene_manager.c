#include "scene_manager.h"
#include "enroll/enroll_scene.h"
#include "settings/settings_scene.h"
#include "settings/passcode_canvas.h"
#include "cards/cards_scene.h"
#include "cards/edit_callbacks.h"
#include "../settings/nfc_login_settings.h"
#include "../storage/nfc_login_card_storage.h"
#include "../scan/nfc_login_scan.h"
#include "../hid/nfc_login_hid.h"
#include "../crypto/nfc_login_passcode.h"


static bool app_navigation_callback(void* context);
static void app_file_browser_callback(void* context);
static void submenu_callback(void* context, uint32_t index);
static void app_delete_card_at_index(App* app, size_t index);
static bool app_handle_edit_menu_input(App* app, InputEvent* event);
static void app_navigate_card_list_up(App* app);
static void app_navigate_card_list_down(App* app);

#define IS_PASSCODE_STATE(ws) ((ws) == 7 || (ws) == 6)
#define STRNCPY_SAFE(dst, src, size) do { \
    strncpy((dst), (src), (size) - 1); \
    (dst)[(size) - 1] = '\0'; \
} while(0)

void app_switch_to_view(App* app, uint32_t view_id) {
    if(!app || !app->view_dispatcher) {
        return;
    }
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

void scene_manager_init(App* app) {
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_callback);

    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Add NFC Card", SubmenuAddCard, submenu_callback, app);
    submenu_add_item(app->submenu, "List Cards", SubmenuListCards, submenu_callback, app);
    submenu_add_item(app->submenu, "Start Scan", SubmenuStartScan, submenu_callback, app);
    submenu_add_item(app->submenu, "Settings", SubmenuSettings, submenu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
#ifdef HAS_MOMENTUM_SUPPORT
    if(app->text_input) text_input_show_illegal_symbols(app->text_input, true);
#endif
    view_dispatcher_add_view(app->view_dispatcher, ViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    View* widget_view = widget_get_view(app->widget);
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_view);
    view_set_input_callback(widget_view, app_widget_view_input_handler);
    view_set_context(widget_view, app);

    app->fb_output_path = furi_string_alloc();
    app->file_browser = file_browser_alloc(app->fb_output_path);
    file_browser_configure(
        app->file_browser,
        ".nfc",
        "/ext/nfc",
        true,
        true,
        NULL,
        false);
    file_browser_set_callback(app->file_browser, app_file_browser_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewFileBrowser, file_browser_get_view(app->file_browser));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewByteInput, byte_input_get_view(app->byte_input));

    app->passcode_canvas_view = passcode_canvas_view_alloc(app);
    if(app->passcode_canvas_view) {
        view_set_context(app->passcode_canvas_view, app);
        view_dispatcher_add_view(app->view_dispatcher, ViewPasscodeCanvas, app->passcode_canvas_view);
    }
}

static bool app_navigation_callback(void* context) {
    App* app = context;
    
    if(app->passcode_prompt_active && app->widget_state == 7) {
        return true;
    }
    
    if(app->current_view == ViewSubmenu) {
        if(app->enrollment_scanning) {
            app->enrollment_scanning = false;
            if(app->enroll_scan_thread) {
                furi_thread_join(app->enroll_scan_thread);
                furi_thread_free(app->enroll_scan_thread);
                app->enroll_scan_thread = NULL;
            }
        }
        if(app->scanning) {
            app->scanning = false;
            // Wait a bit for scan thread to exit HID operations
            furi_delay_ms(SCENE_DELAY_MS);
            if(app->scan_thread) {
                furi_thread_join(app->scan_thread);
                furi_thread_free(app->scan_thread);
                app->scan_thread = NULL;
            }
            if(app->previous_usb_config || app->hid_mode == HidModeBle) {
                deinitialize_hid_with_restore_and_mode(app->previous_usb_config, app->hid_mode);
                app->previous_usb_config = NULL;
            }
        }
        // Save settings before exiting app
        app_save_settings(app);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(app->current_view == ViewTextInput) {
        if(app->edit_state != EditStateNone) {
            app->edit_state = EditStateNone;
            app->widget_state = 3;
            app_render_edit_menu(app);
            app_switch_to_view(app, ViewWidget);
        } else if(app->enrollment_state != EnrollmentStateNone) {
            app->enrollment_state = EnrollmentStateNone;
            app_switch_to_view(app, ViewSubmenu);
        } else {
            app_switch_to_view(app, ViewSubmenu);
        }
        return true;
    } else if(app->current_view == ViewFileBrowser) {
        file_browser_stop(app->file_browser);
        app_switch_to_view(app, ViewWidget);
        return true;
    } else if(app->current_view == ViewByteInput) {
        if(app->enrollment_state != EnrollmentStateNone) {
            app->enrollment_state = EnrollmentStateNone;
            app_switch_to_view(app, ViewSubmenu);
        } else {
            app->edit_state = EditStateNone;
            app->widget_state = 3;
            app_render_edit_menu(app);
            app_switch_to_view(app, ViewWidget);
        }
        return true;
    } else if(app->current_view == ViewWidget) {
        if(app->passcode_prompt_active && IS_PASSCODE_STATE(app->widget_state)) {
            return true;
        }
        
        if(app->enrollment_scanning) {
            app->enrollment_scanning = false;
            if(app->enroll_scan_thread) {
                furi_thread_join(app->enroll_scan_thread);
                furi_thread_free(app->enroll_scan_thread);
                app->enroll_scan_thread = NULL;
            }
        }
        if(app->scanning) {
            app->scanning = false;
            furi_delay_ms(100);
            if(app->scan_thread) {
                furi_thread_join(app->scan_thread);
                furi_thread_free(app->scan_thread);
                app->scan_thread = NULL;
            }
            if(app->previous_usb_config || app->hid_mode == HidModeBle) {
                deinitialize_hid_with_restore_and_mode(app->previous_usb_config, app->hid_mode);
                app->previous_usb_config = NULL;
            }
        }
        if(app->widget_state == 4) {
            app_save_settings(app);
        }
        app_switch_to_view(app, ViewSubmenu);
        return true;
    }
    return false;
}

static void app_file_browser_callback(void* context) {
    App* app = context;
    const char* path = furi_string_get_cstr(app->fb_output_path);

    if(app->selecting_keyboard_layout) {
        if(!path || path[0] == '\0') {
            app->selecting_keyboard_layout = false;
            app->widget_state = 4;
            app_render_settings(app);
            app_switch_to_view(app, ViewWidget);
            return;
        }

        const char* filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;

        if(strstr(filename, ".kl") != NULL) {
            STRNCPY_SAFE(app->keyboard_layout, filename, sizeof(app->keyboard_layout));
            app_save_settings(app);
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
        }

        app->selecting_keyboard_layout = false;
        app->widget_state = 4;
        app_render_settings(app);
        app_switch_to_view(app, ViewWidget);
        return;
    }

    if(!path || path[0] == '\0') {
        app_switch_to_view(app, ViewWidget);
        return;
    }

    if(app_import_nfc_file(app, path)) {
        app->enrollment_state = EnrollmentStatePassword;
        memset(app->enrollment_card.password, 0, sizeof(app->enrollment_card.password));
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Enter Password");
        text_input_set_result_callback(
            app->text_input,
            app_text_input_result_callback,
            app,
            app->enrollment_card.password,
            sizeof(app->enrollment_card.password),
            true);
#ifdef HAS_MOMENTUM_SUPPORT
        text_input_show_illegal_symbols(app->text_input, true);
#endif
        app_switch_to_view(app, ViewTextInput);
    } else {
        notification_message(app->notification, &sequence_error);
        app_switch_to_view(app, ViewWidget);
    }
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case SubmenuAddCard:
        view_dispatcher_send_custom_event(app->view_dispatcher, EventAddCardStart);
        break;
    case SubmenuListCards:
        app->widget_state = 2;
        app->selected_card = 0;
        app->card_list_scroll_offset = 0;
        app_render_card_list(app);
        app_switch_to_view(app, ViewWidget);
        break;
    case SubmenuStartScan:
        if(!app->scanning) {
            app->scanning = true;
            app->scan_thread = furi_thread_alloc();
            furi_thread_set_name(app->scan_thread, "NfcScan");
            furi_thread_set_stack_size(app->scan_thread, 4 * 1024);
            furi_thread_set_context(app->scan_thread, app);
            furi_thread_set_callback(app->scan_thread, app_scan_thread);
            furi_thread_start(app->scan_thread);

            widget_reset(app->widget);
            app->widget_state = 1;
            widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Scanning for NFC...");
            widget_add_string_element(app->widget, 0, 20, AlignLeft, AlignTop, FontSecondary, "Hold card to reader");
            widget_add_string_element(app->widget, 0, 40, AlignLeft, AlignTop, FontSecondary, "Press Back to stop");
            app_switch_to_view(app, ViewWidget);
        }
        break;
    case SubmenuSettings:
        app->widget_state = 4;
        app->settings_menu_index = 0;
        app->settings_scroll_offset = 0;
        app->selecting_keyboard_layout = false;
        app_render_settings(app);
        app_switch_to_view(app, ViewWidget);
        break;
    }
}

bool app_widget_view_input_handler(InputEvent* event, void* context) {
    App* app = context;
    
    // Handle Back button in lockscreen (widget_state == 7) and setup (widget_state == 6) FIRST
    // This MUST be checked before any other Back handlers
    if(IS_PASSCODE_STATE(app->widget_state) && event->key == InputKeyBack) {
        if(event->type == InputTypeLong) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        return true;
    }
    
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyBack) {
            if(app->scanning) {
                app->scanning = false;
                // Wait a bit for scan thread to exit HID operations
                furi_delay_ms(100);
            if(app->scan_thread) {
                furi_thread_join(app->scan_thread);
                furi_thread_free(app->scan_thread);
                app->scan_thread = NULL;
            }
            if(app->previous_usb_config || app->hid_mode == HidModeBle) {
                    deinitialize_hid_with_restore_and_mode(app->previous_usb_config, app->hid_mode);
                    app->previous_usb_config = NULL;
                }
            } else if(app->enrollment_scanning) {
                app->enrollment_scanning = false;
                if(app->enroll_scan_thread) {
                    furi_thread_join(app->enroll_scan_thread);
                    furi_thread_free(app->enroll_scan_thread);
                    app->enroll_scan_thread = NULL;
                }
                app->widget_state = 0;
            } else if(app->enrollment_state != EnrollmentStateNone) {
                app->enrollment_state = EnrollmentStateNone;
            } else if(app->widget_state == 4) {
                app_save_settings(app);
                app->widget_state = 0;
            }
            app_switch_to_view(app, ViewSubmenu);
            return true;
        }
        if(app->widget_state == 1 && app->enrollment_scanning) {
            // During enrollment scanning, only Right triggers manual UID entry
            if(event->key == InputKeyRight) {
                view_dispatcher_send_custom_event(app->view_dispatcher, EventManualUidEntry);
                return true;
            }
        }
        if(app->widget_state == 4) {
            if(event->key == InputKeyUp) {
                if(app->settings_menu_index > 0) {
                    app->settings_menu_index--;
                    if(app->settings_menu_index < app->settings_scroll_offset) {
                        app->settings_scroll_offset = app->settings_menu_index;
                    }
                }
            } else if(event->key == InputKeyDown) {
                if(app->settings_menu_index < SETTINGS_MENU_ITEMS - 1) {
                    app->settings_menu_index++;
                    if(app->settings_menu_index >= app->settings_scroll_offset + SETTINGS_VISIBLE_ITEMS) {
                        app->settings_scroll_offset = app->settings_menu_index - (SETTINGS_VISIBLE_ITEMS - 1);
                    }
                }
            } else if(event->key == InputKeyOk) {
                if(app->settings_menu_index == 0) {
                    // HID Mode toggle - handled by Left/Right
                    // This is just a placeholder, actual toggle is in Left/Right handler
                } else if(app->settings_menu_index == 1) {
                    app->selecting_keyboard_layout = true;
                    furi_string_set(app->fb_output_path, BADUSB_LAYOUTS_DIR);
                    file_browser_configure(
                        app->file_browser,
                        ".kl",
                        BADUSB_LAYOUTS_DIR,
                        true,
                        true,
                        NULL,
                        false);
                    file_browser_start(app->file_browser, app->fb_output_path);
                    app_switch_to_view(app, ViewFileBrowser);
                    return true;
                } else if(app->settings_menu_index == 2) {
                } else if(app->settings_menu_index == 3) {
                    app->append_enter = !app->append_enter;
                    app_save_settings(app);
                    notification_message(app->notification, &sequence_success);
                    app_render_settings(app);
                    return true;
                } else if(app->settings_menu_index == 4) {
                    app_load_cards(app);
                    size_t cards_loaded = app->card_count;
                    
                    delete_cards_and_reset_passcode();
                    
                    app->passcode_failed_attempts = 0;
                    app->passcode_sequence_len = 0;
                    memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                    
                    if(cards_loaded == 0) {
                        app->card_count = 0;
                    }
                    
                    app->widget_state = 6;
                    app->passcode_prompt_active = true;
                    app->passcode_sequence_len = 0;
                    memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                    app_switch_to_view(app, ViewPasscodeCanvas);
                    notification_message(app->notification, &sequence_success);
                    return true;
                } else if(app->settings_menu_index == 5) {
                    bool current_state = get_passcode_disabled();
                    if(set_passcode_disabled(!current_state)) {
                        app_save_settings(app);
                        notification_message(app->notification, &sequence_success);
                    } else {
                        notification_message(app->notification, &sequence_error);
                    }
                    app_render_settings(app);
                    return true;
                } else if(app->settings_menu_index == 6) {
                    app->widget_state = 5;
                    app->credits_page = 0;
                    widget_reset(app->widget);
                    app_render_credits(app);
                    return true;
                }
            } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
                if(app->settings_menu_index == 0) {
                    HidMode old_mode = app->hid_mode;
                    app->hid_mode = (app->hid_mode == HidModeUsb) ? HidModeBle : HidModeUsb;
                    
                    if(old_mode == HidModeUsb && app->hid_mode == HidModeBle) {
                        app_start_ble_advertising();
                    } else if(old_mode == HidModeBle && app->hid_mode == HidModeUsb) {
                        app_stop_ble_advertising();
                    }
                    
                    app_save_settings(app);
                    notification_message(app->notification, &sequence_success);
                } else if(app->settings_menu_index == 1) {
                    Storage* storage = furi_record_open(RECORD_STORAGE);
                    File* dir = storage_file_alloc(storage);
                    char layouts[MAX_LAYOUTS][64];
                    size_t layout_count = 0;
                    const char* current_layout = app->keyboard_layout;
                    int current_index = -1;

                    if(storage_dir_open(dir, BADUSB_LAYOUTS_DIR)) {
                        FileInfo file_info;
                        char name[64];
                        while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
                            const char* ext = strrchr(name, '.');
                            if(ext && strcmp(ext, ".kl") == 0) {
                                if(layout_count < MAX_LAYOUTS) {
                                    STRNCPY_SAFE(layouts[layout_count], name, sizeof(layouts[0]));
                                    if(strcmp(layouts[layout_count], current_layout) == 0) {
                                        current_index = (int)layout_count;
                                    }
                                    layout_count++;
                                }
                            }
                        }
                        storage_dir_close(dir);
                    }
                    storage_file_free(dir);
                    furi_record_close(RECORD_STORAGE);

                    if(layout_count < MAX_LAYOUTS) {
                        // Shift existing layouts down by one
                        for(size_t i = layout_count; i > 0; i--) {
                            STRNCPY_SAFE(layouts[i], layouts[i-1], sizeof(layouts[0]));
                        }
                        // Insert default layout at position 0
                        STRNCPY_SAFE(layouts[0], "en-US.kl", sizeof(layouts[0]));
                        layout_count++;
                        // Adjust current_index if needed
                        if(current_index >= 0) {
                            current_index++;
                        } else if(strcmp(current_layout, "en-US.kl") == 0) {
                            current_index = 0;
                        }
                    }

                    if(layout_count > 0) {
                        if(current_index < 0) current_index = 0;

                        if(event->key == InputKeyLeft) {
                            current_index = (current_index > 0) ? current_index - 1 : (int)(layout_count - 1);
                        } else {
                            current_index = (int)((current_index + 1) % layout_count);
                        }

                        STRNCPY_SAFE(app->keyboard_layout, layouts[current_index], sizeof(app->keyboard_layout));
                        app_save_settings(app);
                    notification_message(app->notification, &sequence_success);
                }
                } else if(app->settings_menu_index == 2) {
                    const uint16_t delays[] = {10, 50, 100, 200};
                    uint8_t current_idx = 0;
                    for(uint8_t i = 0; i < 4; i++) {
                        if(delays[i] == app->input_delay_ms) {
                            current_idx = i;
                            break;
                        }
                    }
                    if(event->key == InputKeyLeft) {
                        current_idx = (current_idx > 0) ? current_idx - 1 : 3;
                    } else {
                        current_idx = (current_idx + 1) % 4;
                    }
                    app->input_delay_ms = delays[current_idx];
                    app_save_settings(app);
                    notification_message(app->notification, &sequence_success);
                } else if(app->settings_menu_index == 3) {
                    app->append_enter = !app->append_enter;
                    app_save_settings(app);
                    notification_message(app->notification, &sequence_success);
                } else if(app->settings_menu_index == 5) {
                    bool current_state = get_passcode_disabled();
                    if(set_passcode_disabled(!current_state)) {
                        app_save_settings(app);
                        notification_message(app->notification, &sequence_success);
                    } else {
                        notification_message(app->notification, &sequence_error);
                    }
                }
            }
            app_render_settings(app);
            return true;
        }
        if(app->widget_state == 5) {
            if(event->key == InputKeyLeft) {
                if(app->credits_page > 0) {
                    app->credits_page--;
                } else {
                    app->credits_page = CREDITS_PAGES - 1;
                }
                app_render_credits(app);
                return true;
            } else if(event->key == InputKeyRight) {
                if(app->credits_page < CREDITS_PAGES - 1) {
                    app->credits_page++;
                } else {
                    app->credits_page = 0;
                }
                app_render_credits(app);
                return true;
            } else if(event->key == InputKeyBack) {
                app->widget_state = 4;
                app_render_settings(app);
                return true;
            }
            return true;
        }
        if(app->widget_state == 6) {
            if(event->key == InputKeyBack) {
                if(event->type == InputTypeLong) {
                    view_dispatcher_stop(app->view_dispatcher);
                    return true;
                }
                return true;
            }
            
            if(event->type == InputTypeShort) {
                const char* button_name = NULL;
                bool add_button = false;
                
                if(event->key == InputKeyUp) {
                    button_name = "up";
                    add_button = true;
                } else if(event->key == InputKeyDown) {
                    button_name = "down";
                    add_button = true;
                } else if(event->key == InputKeyLeft) {
                    button_name = "left";
                    add_button = true;
                } else if(event->key == InputKeyRight) {
                    button_name = "right";
                    add_button = true;
                } else if(event->key == InputKeyOk) {
                    if(app->passcode_sequence_len == 0) {
                        app->widget_state = 6;
                        app_switch_to_view(app, ViewPasscodeCanvas);
                        return true;
                    }
                    
                    app->passcode_sequence[app->passcode_sequence_len] = '\0';
                    size_t button_count = count_buttons_in_sequence(app->passcode_sequence);
                    
                    if(button_count < MIN_PASSCODE_BUTTONS || button_count > MAX_PASSCODE_BUTTONS) {
                        widget_reset(app->widget);
                        widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Setup Passcode");
                        char error_msg[64];
                        snprintf(error_msg, sizeof(error_msg), "Need %d-%d buttons", MIN_PASSCODE_BUTTONS, MAX_PASSCODE_BUTTONS);
                        widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, error_msg);
                        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Press any button");
                        notification_message(app->notification, &sequence_error);
                        app->passcode_sequence_len = 0;
                        memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                        return true;
                    }
                    
                    if(set_passcode_sequence(app->passcode_sequence)) {
                        notification_message(app->notification, &sequence_success);
                        
                        furi_delay_ms(100);
                        
                        if(app->card_count > 0) {
                            if(!app_save_cards(app)) {
                                FURI_LOG_E(TAG, "app_widget_view_input: Failed to save preserved cards");
                            }
                        } else {
                            app_load_cards(app);
                        }
                        
                        app->passcode_sequence_len = 0;
                        memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                        app->widget_state = 0;
                        
                        app->passcode_prompt_active = false;
                        app_switch_to_view(app, ViewSubmenu);
                        return true;
                    } else {
                        notification_message(app->notification, &sequence_error);
                        widget_reset(app->widget);
                        widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Setup Passcode");
                        widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, "Failed to save");
                        widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Press any button");
                        app->passcode_sequence_len = 0;
                        memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                    }
                    return true;
                } else if(event->key == InputKeyBack && event->type == InputTypeLong) {
                    view_dispatcher_stop(app->view_dispatcher);
                    return true;
                }
                
                if(add_button && button_name) {
                    size_t current_count = count_buttons_in_sequence(app->passcode_sequence);
                    
                    if(current_count < MAX_PASSCODE_BUTTONS) {
                        size_t name_len = strlen(button_name);
                        if(app->passcode_sequence_len + name_len + 2 < MAX_PASSCODE_SEQUENCE_LEN) {
                            if(app->passcode_sequence_len > 0) {
                                app->passcode_sequence[app->passcode_sequence_len++] = ' ';
                            }
                            strcpy(app->passcode_sequence + app->passcode_sequence_len, button_name);
                            app->passcode_sequence_len += name_len;
                        }
                    } else {
                        // Max buttons reached - show notification
                        notification_message(app->notification, &sequence_error);
                    }
                }
                
                // Canvas view will redraw automatically
                // Just ensure we're on the canvas view
                if(app->current_view != ViewPasscodeCanvas) {
                    app_switch_to_view(app, ViewPasscodeCanvas);
                }
                return true;
            }
            return true;
        }
        
        if(app->widget_state == 7) {
            if(event->type == InputTypeShort) {
                const char* button_name = NULL;
                bool add_button = false;
                
                if(event->key == InputKeyUp) {
                    button_name = "up";
                    add_button = true;
                } else if(event->key == InputKeyDown) {
                    button_name = "down";
                    add_button = true;
                } else if(event->key == InputKeyLeft) {
                    button_name = "left";
                    add_button = true;
                } else if(event->key == InputKeyRight) {
                    button_name = "right";
                    add_button = true;
                } else if(event->key == InputKeyOk) {
                    if(app->passcode_sequence_len > 0) {
                        app->passcode_sequence[app->passcode_sequence_len] = '\0';
                        
                        if(verify_passcode_sequence(app->passcode_sequence)) {
                            app->passcode_failed_attempts = 0;
                            app_load_cards(app);
                            notification_message(app->notification, &sequence_success);
                            
                            app->passcode_sequence_len = 0;
                            memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                            app->widget_state = 0;
                            
                            app->passcode_prompt_active = false;
                            app_switch_to_view(app, ViewSubmenu);
                            return true;
                        } else {
                            app->passcode_failed_attempts++;
                            FURI_LOG_W(TAG, "app_widget_view_input: Wrong passcode (attempt %u/5)", app->passcode_failed_attempts);
                            
                            if(app->passcode_failed_attempts >= 5) {
                                FURI_LOG_E(TAG, "app_widget_view_input: 5 failed attempts - deleting cards.enc and resetting passcode");
                                delete_cards_and_reset_passcode();
                                
                                app->passcode_failed_attempts = 0;
                                app->passcode_sequence_len = 0;
                                memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                                app->card_count = 0;
                                
                                app->widget_state = 6;
                                widget_reset(app->widget);
                                widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Security Reset");
                                widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, "Cards deleted");
                                widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "Press OK to continue");
                                app_switch_to_view(app, ViewWidget);
                                notification_message(app->notification, &sequence_error);
                            } else {
                                app->passcode_sequence_len = 0;
                                memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
                                notification_message(app->notification, &sequence_error);
                                
                                if(app->current_view != ViewPasscodeCanvas) {
                                    app_switch_to_view(app, ViewPasscodeCanvas);
                                }
                            }
                        }
                    }
                    return true;
                }
                
                if(add_button && button_name) {
                    char stored_sequence[MAX_PASSCODE_SEQUENCE_LEN];
                    size_t stored_button_count = MAX_PASSCODE_BUTTONS;
                    if(get_passcode_sequence(stored_sequence, sizeof(stored_sequence))) {
                        stored_button_count = count_buttons_in_sequence(stored_sequence);
                    }
                    
                    size_t current_count = count_buttons_in_sequence(app->passcode_sequence);
                    
                    if(current_count < stored_button_count) {
                        size_t name_len = strlen(button_name);
                        if(app->passcode_sequence_len + name_len + 2 < MAX_PASSCODE_SEQUENCE_LEN) {
                            if(app->passcode_sequence_len > 0) {
                                app->passcode_sequence[app->passcode_sequence_len++] = ' ';
                            }
                            strcpy(app->passcode_sequence + app->passcode_sequence_len, button_name);
                            app->passcode_sequence_len += name_len;
                        }
                    } else {
                        notification_message(app->notification, &sequence_error);
                    }
                    
                    if(app->current_view != ViewPasscodeCanvas) {
                        app_switch_to_view(app, ViewPasscodeCanvas);
                    }
                }
                return true;
            }
            return true;
        }
        if(app->widget_state == 3) {
            if(app_handle_edit_menu_input(app, event)) {
                return true;
            }
        }
        if(app->widget_state == 2) {
            if(event->key == InputKeyRight) {
                // Allow import even when card list is empty
                furi_string_set(app->fb_output_path, "/ext/nfc");
                file_browser_start(app->file_browser, app->fb_output_path);
                app_switch_to_view(app, ViewFileBrowser);
                return true;
            }
            if(app->card_count == 0) {
                return true;
            }
            if(event->key == InputKeyUp) {
                app_navigate_card_list_up(app);
            } else if(event->key == InputKeyDown) {
                app_navigate_card_list_down(app);
            } else if(event->key == InputKeyOk) {
                app->has_active_selection = true;
                app->active_card_index = app->selected_card;
                app_save_settings(app);
                notification_message(app->notification, &sequence_success);
            }
            app_render_card_list(app);
            return true;
        }
    } else if(event->type == InputTypeLong) {
        if(app->widget_state == 2 && event->key == InputKeyOk && app->card_count > 0) {
            app->edit_card_index = app->selected_card;
            app->edit_menu_index = 0;
            app->widget_state = 3;
            app->just_entered_edit_mode = true;
            app_render_edit_menu(app);
            return true;
        }
        if(app->widget_state == 3) {
            if(app_handle_edit_menu_input(app, event)) {
                return true;
            }
        }
    }
    return false;
}

static void app_delete_card_at_index(App* app, size_t index) {
    if(index >= app->card_count) return;
    
    for(size_t i = index; i + 1 < app->card_count; i++) {
        app->cards[i] = app->cards[i + 1];
    }
    app->card_count--;
    
    if(app->has_active_selection) {
        if(app->active_card_index == index) {
            app->has_active_selection = false;
        } else if(app->active_card_index > index) {
            app->active_card_index--;
        }
    }
    
    if(app_save_cards(app)) {
        notification_message(app->notification, &sequence_success);
    } else {
        notification_message(app->notification, &sequence_error);
        FURI_LOG_E(TAG, "Failed to save after card deletion");
    }
}

static void app_navigate_card_list_up(App* app) {
    if(app->selected_card > 0) {
        app->selected_card--;
        if(app->selected_card < (size_t)app->card_list_scroll_offset) {
            app->card_list_scroll_offset = (uint8_t)app->selected_card;
        }
    } else {
        app->selected_card = app->card_count - 1;
        if(app->card_count > CARD_LIST_VISIBLE_ITEMS) {
            app->card_list_scroll_offset = (uint8_t)(app->selected_card - (CARD_LIST_VISIBLE_ITEMS - 1));
        } else {
            app->card_list_scroll_offset = 0;
        }
    }
}

static void app_navigate_card_list_down(App* app) {
    if(app->selected_card + 1 < app->card_count) {
        app->selected_card++;
        if(app->selected_card >= (size_t)(app->card_list_scroll_offset + CARD_LIST_VISIBLE_ITEMS)) {
            app->card_list_scroll_offset = (uint8_t)(app->selected_card - (CARD_LIST_VISIBLE_ITEMS - 1));
        }
    } else {
        app->selected_card = 0;
        app->card_list_scroll_offset = 0;
    }
}

static bool app_handle_edit_menu_input(App* app, InputEvent* event) {
    bool was_just_entered = app->just_entered_edit_mode;
    app->just_entered_edit_mode = false;
    
    if(event->key == InputKeyUp) {
        if(app->edit_menu_index > 0) app->edit_menu_index--;
    } else if(event->key == InputKeyDown) {
        if(app->edit_menu_index < 3) app->edit_menu_index++;
    } else if(event->key == InputKeyBack) {
        app->widget_state = 2;
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(was_just_entered) {
            app_render_edit_menu(app);
            return true;
        }
        if(app->edit_menu_index == 0) {
            app->edit_state = EditStateName;
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Edit Name");
            text_input_set_result_callback(
                app->text_input,
                app_edit_text_result_callback,
                app,
                app->cards[app->edit_card_index].name,
                sizeof(app->cards[app->edit_card_index].name),
                false);
#ifdef HAS_MOMENTUM_SUPPORT
            text_input_show_illegal_symbols(app->text_input, true);
#endif
            app_switch_to_view(app, ViewTextInput);
            return true;
        } else if(app->edit_menu_index == 1) {
            app->edit_state = EditStatePassword;
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Edit Password");
            text_input_set_result_callback(
                app->text_input,
                app_edit_text_result_callback,
                app,
                app->cards[app->edit_card_index].password,
                sizeof(app->cards[app->edit_card_index].password),
                false);
#ifdef HAS_MOMENTUM_SUPPORT
            text_input_show_illegal_symbols(app->text_input, true);
#endif
            app_switch_to_view(app, ViewTextInput);
            return true;
        } else if(app->edit_menu_index == 2) {
            app->edit_state = EditStateUid;
            memset(app->edit_uid_bytes, 0, sizeof(app->edit_uid_bytes));
            if(app->cards[app->edit_card_index].uid_len > 0 && 
               app->cards[app->edit_card_index].uid_len <= MAX_UID_LEN) {
                app->edit_uid_len = app->cards[app->edit_card_index].uid_len;
                memcpy(app->edit_uid_bytes, app->cards[app->edit_card_index].uid, 
                       app->cards[app->edit_card_index].uid_len);
            } else {
                app->edit_uid_len = 4;
            }
            byte_input_set_header_text(app->byte_input, "Edit UID (hex)");
            byte_input_set_result_callback(
                app->byte_input,
                app_edit_uid_byte_input_done,
                NULL,
                app,
                app->edit_uid_bytes,
                app->edit_uid_len);
            app_switch_to_view(app, ViewByteInput);
            return true;
        } else if(app->edit_menu_index == 3) {
            app_delete_card_at_index(app, app->edit_card_index);
            app->widget_state = 2;
        }
    }
    
    if(app->widget_state == 3) {
        app_render_edit_menu(app);
        return true;
    }
    if(app->widget_state == 2) {
        app_render_card_list(app);
        return true;
    }
    return true;
}