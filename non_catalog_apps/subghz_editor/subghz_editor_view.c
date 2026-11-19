#include "subghz_editor_view.h"
#include "subghz_editor_file.h"
#include <stdlib.h>
#include <string.h>

#define TAG "SubGhzEditorView"

static bool subghz_editor_parse_frequency(const char* str, uint32_t* out_freq) {
    if(!str || !out_freq) return false;

    // Skip leading whitespaces
    while(*str == ' ' || *str == '\t')
        str++;
    if(!*str) return false;

    // Check if decimal point/comma exists (e.g. 433.92, 433,92 MHz)
    const char* dot = strchr(str, '.');
    if(!dot) dot = strchr(str, ',');

    if(dot) {
        char clean[32];
        strncpy(clean, str, sizeof(clean) - 1);
        clean[sizeof(clean) - 1] = '\0';
        char* cdot = strchr(clean, ',');
        if(cdot) *cdot = '.';

        char* endptr = NULL;
        float mhz = strtof(clean, &endptr);
        if(mhz <= 0.0f) return false;
        *out_freq = (uint32_t)(mhz * 1000000.0f + 0.5f);
        return true;
    } else {
        char* endptr = NULL;
        unsigned long val = strtoul(str, &endptr, 10);
        if(val == 0) return false;

        if(val < 10000) {
            // e.g. 433 -> 433 MHz
            val *= 1000000;
        } else if(val < 10000000) {
            // e.g. 433920 -> 433.92 MHz
            val *= 1000;
        }
        *out_freq = (uint32_t)val;
        return true;
    }
}

static void subghz_editor_freq_change(VariableItem* item) {
    SubGhzEditorApp* app = (SubGhzEditorApp*)variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(app->model.is_custom_freq) {
        if(index == 0) {
            variable_item_set_current_value_text(item, app->model.custom_freq_str);
            app->model.frequency = app->model.initial_frequency;
        } else {
            uint8_t p_idx = index - 1;
            variable_item_set_current_value_text(item, subghz_presets[p_idx].str);
            app->model.frequency = subghz_presets[p_idx].frequency;
        }
    } else {
        variable_item_set_current_value_text(item, subghz_presets[index].str);
        app->model.frequency = subghz_presets[index].frequency;
    }

    app->model.current_freq_index = index;
    app->model.modified = true;
    FURI_LOG_I(TAG, "Frequency changed via preset: %lu Hz", (unsigned long)app->model.frequency);
}

static void subghz_editor_freq_input_done(void* context) {
    SubGhzEditorApp* app = (SubGhzEditorApp*)context;
    uint32_t parsed_freq = 0;

    FURI_LOG_I(TAG, "Manual frequency input submitted: %s", app->model.freq_input_buf);

    if(!subghz_editor_parse_frequency(app->model.freq_input_buf, &parsed_freq) ||
       !furi_hal_subghz_is_frequency_valid(parsed_freq)) {
        FURI_LOG_W(
            TAG,
            "Invalid/out-of-range frequency: %s (parsed=%lu)",
            app->model.freq_input_buf,
            (unsigned long)parsed_freq);
        notification_message(app->notifications, &sequence_error);
        subghz_editor_show_message(
            app,
            "Out of Range!",
            "Frequency is not allowed!\n\nAllowed bands:\n300 - 348 MHz\n387 - 464 MHz\n779 - 928 MHz");
        return;
    }

    FURI_LOG_I(TAG, "Frequency accepted: %lu Hz", (unsigned long)parsed_freq);
    app->model.frequency = parsed_freq;

    // Check if matches standard preset
    bool match = false;
    for(size_t i = 0; i < SUBGHZ_PRESETS_COUNT; i++) {
        if(subghz_presets[i].frequency == app->model.frequency) {
            app->model.current_freq_index = (uint8_t)i;
            app->model.is_custom_freq = false;
            match = true;
            break;
        }
    }
    if(!match) {
        app->model.is_custom_freq = true;
        snprintf(
            app->model.custom_freq_str,
            sizeof(app->model.custom_freq_str),
            "%lu.%02lu MHz",
            (unsigned long)(app->model.frequency / 1000000),
            (unsigned long)((app->model.frequency % 1000000) / 10000));
        app->model.current_freq_index = 0;
    }

    app->model.modified = true;
    notification_message(app->notifications, &sequence_success);

    subghz_editor_view_refresh_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewVariableList);
}

static void subghz_editor_text_input_done(void* context) {
    SubGhzEditorApp* app = (SubGhzEditorApp*)context;
    snprintf(app->model.key_buf, sizeof(app->model.key_buf), "%s", app->model.temp_key_buf);
    app->model.modified = true;
    FURI_LOG_I(TAG, "Key input updated: %s", app->model.key_buf);

    subghz_editor_view_refresh_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewVariableList);
}

static void subghz_editor_list_enter(void* context, uint32_t index) {
    SubGhzEditorApp* app = (SubGhzEditorApp*)context;
    FURI_LOG_I(
        TAG,
        "List item pressed: %lu (file_loaded=%d)",
        (unsigned long)index,
        app->model.is_file_loaded);

    if(!app->model.is_file_loaded) {
        if(index == 0) {
            // First item: Open .sub File (dispatch event)
            FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventOpenFile");
            view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventOpenFile);
        } else if(index == 1) {
            // Second item: Help / About
            FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventAbout");
            view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventAbout);
        }
        return;
    }

    // File loaded layout:
    // 0: File [Select another file]
    // 1: Frequency [Left/Right presets, OK opens manual editor]
    // 2 (if key): Key [OK opens HEX editor]
    // 3: Protocol [Read-only info]
    // 4: Save (Overwrite)
    // 5: Save As Copy
    // 6: Close File

    uint32_t current_idx = 0;

    // Item 0: File [Select another file]
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventOpenFile (switch file)");
        view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventOpenFile);
        return;
    }
    current_idx++;

    // Item 1: Frequency (pressing OK opens manual frequency editor)
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Opening manual frequency editor");
        snprintf(
            app->model.freq_input_buf,
            sizeof(app->model.freq_input_buf),
            "%lu",
            (unsigned long)app->model.frequency);
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Freq (Hz or MHz):");
        text_input_set_result_callback(
            app->text_input,
            subghz_editor_freq_input_done,
            app,
            app->model.freq_input_buf,
            sizeof(app->model.freq_input_buf),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewTextInput);
        return;
    }
    current_idx++;

    // Item 2: Key (if available)
    if(app->model.has_key) {
        if(index == current_idx) {
            FURI_LOG_I(TAG, "Switching to TextInput for HEX key edit");
            snprintf(
                app->model.temp_key_buf, sizeof(app->model.temp_key_buf), "%s", app->model.key_buf);
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Edit Key (HEX):");
            text_input_set_result_callback(
                app->text_input,
                subghz_editor_text_input_done,
                app,
                app->model.temp_key_buf,
                sizeof(app->model.temp_key_buf),
                false);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewTextInput);
            return;
        }
        current_idx++;
    }

    // Protocol info item (read only)
    if(index == current_idx) {
        return;
    }
    current_idx++;

    // Save Overwrite
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventSaveOverwrite");
        view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventSaveOverwrite);
        return;
    }
    current_idx++;

    // Save As Copy
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventSaveCopy");
        view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventSaveCopy);
        return;
    }
    current_idx++;

    // Close File
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventCloseFile");
        view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventCloseFile);
        return;
    }
    current_idx++;

    // Help / About
    if(index == current_idx) {
        FURI_LOG_I(TAG, "Dispatching SubGhzEditorEventAbout");
        view_dispatcher_send_custom_event(app->view_dispatcher, SubGhzEditorEventAbout);
        return;
    }
}

bool subghz_editor_custom_event_callback(void* context, uint32_t event) {
    SubGhzEditorApp* app = (SubGhzEditorApp*)context;
    FURI_LOG_I(TAG, "Processing custom event: %lu", (unsigned long)event);

    switch(event) {
    case SubGhzEditorEventOpenFile: {
        FURI_LOG_I(
            TAG, "Opening file browser dialog at %s", furi_string_get_cstr(app->model.file_path));

        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, SUBGHZ_EDITOR_APP_EXTENSION, NULL);
        browser_options.base_path = SUBGHZ_EDITOR_APP_PATH_FOLDER;
        browser_options.hide_ext = false;

        FuriString* preselected_path = furi_string_alloc();
        if(furi_string_size(app->model.file_path) > 0) {
            furi_string_set(preselected_path, app->model.file_path);
        } else {
            furi_string_set_str(preselected_path, SUBGHZ_EDITOR_APP_PATH_FOLDER);
        }

        bool chosen = dialog_file_browser_show(
            app->dialogs, app->model.file_path, preselected_path, &browser_options);

        furi_string_free(preselected_path);

        FURI_LOG_I(
            TAG,
            "File browser closed, chosen=%d, selected_path=%s",
            chosen,
            furi_string_get_cstr(app->model.file_path));

        if(chosen) {
            if(subghz_editor_file_load(app, furi_string_get_cstr(app->model.file_path))) {
                notification_message(app->notifications, &sequence_success);
            } else {
                subghz_editor_show_message(app, "Error", "Failed to load .sub file");
                return true;
            }
        }
        subghz_editor_view_refresh_list(app);
        return true;
    }

    case SubGhzEditorEventAbout: {
        FURI_LOG_I(TAG, "Showing About screen");
        subghz_editor_show_about(app);
        return true;
    }

    case SubGhzEditorEventSaveOverwrite: {
        FURI_LOG_I(TAG, "Saving file (overwrite)...");
        if(subghz_editor_file_save(app, furi_string_get_cstr(app->model.file_path))) {
            notification_message(app->notifications, &sequence_success);
            subghz_editor_show_message(app, "Success", "File updated\nsuccessfully!");
        } else {
            subghz_editor_show_message(app, "Error", "Failed to save file");
        }
        return true;
    }

    case SubGhzEditorEventSaveCopy: {
        FURI_LOG_I(TAG, "Saving file (as copy)...");
        if(subghz_editor_file_save_copy(app)) {
            notification_message(app->notifications, &sequence_success);
            subghz_editor_show_message(app, "Success", "Saved as copy with\n_edit.sub suffix!");
        } else {
            subghz_editor_show_message(app, "Error", "Failed to save copy");
        }
        return true;
    }

    case SubGhzEditorEventCloseFile: {
        FURI_LOG_I(TAG, "Closing file and returning to main menu");
        subghz_editor_model_reset(&app->model);
        subghz_editor_view_refresh_list(app);
        return true;
    }

    default:
        FURI_LOG_W(TAG, "Unknown custom event: %lu", (unsigned long)event);
        return false;
    }
}

void subghz_editor_show_message(SubGhzEditorApp* app, const char* title, const char* msg) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_string_multiline_element(
        app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, msg);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewWidget);
}

void subghz_editor_view_refresh_list(SubGhzEditorApp* app) {
    FURI_LOG_I(TAG, "Refreshing list view (file_loaded=%d)", app->model.is_file_loaded);
    variable_item_list_reset(app->var_list);

    if(!app->model.is_file_loaded) {
        variable_item_list_add(app->var_list, "Open .sub File", 1, NULL, app);
        variable_item_list_add(app->var_list, "Help / About", 1, NULL, app);
        return;
    }

    // 0: File [Select another file]
    VariableItem* file_item = variable_item_list_add(app->var_list, "File:", 1, NULL, app);
    variable_item_set_current_value_text(file_item, furi_string_get_cstr(app->model.file_name));

    // 1: Frequency presets (Left/Right to cycle presets, OK to edit manually)
    uint8_t values_count = SUBGHZ_PRESETS_COUNT;
    if(app->model.is_custom_freq) {
        values_count = SUBGHZ_PRESETS_COUNT + 1;
    }
    VariableItem* freq_item = variable_item_list_add(
        app->var_list, "Freq:", values_count, subghz_editor_freq_change, app);

    variable_item_set_current_value_index(freq_item, app->model.current_freq_index);
    if(app->model.is_custom_freq && app->model.current_freq_index == 0) {
        variable_item_set_current_value_text(freq_item, app->model.custom_freq_str);
    } else {
        uint8_t p_idx = app->model.is_custom_freq ? (app->model.current_freq_index - 1) :
                                                    app->model.current_freq_index;
        variable_item_set_current_value_text(freq_item, subghz_presets[p_idx].str);
    }

    // 2: Key (if available)
    if(app->model.has_key) {
        VariableItem* key_item = variable_item_list_add(app->var_list, "Key:", 1, NULL, app);
        char key_preview[20];
        snprintf(key_preview, sizeof(key_preview), "%.16s", app->model.key_buf);
        variable_item_set_current_value_text(key_item, key_preview);
    }

    // 3: Protocol info (Read-only info)
    VariableItem* proto_item = variable_item_list_add(app->var_list, "Proto:", 1, NULL, app);
    variable_item_set_current_value_text(proto_item, furi_string_get_cstr(app->model.protocol));

    // 4: Save Overwrite
    variable_item_list_add(app->var_list, "Save (Overwrite)", 1, NULL, app);

    // 5: Save As Copy
    variable_item_list_add(app->var_list, "Save As Copy", 1, NULL, app);

    // 6: Close File
    variable_item_list_add(app->var_list, "Close File", 1, NULL, app);

    // 7: Help / About
    variable_item_list_add(app->var_list, "Help / About", 1, NULL, app);
}

void subghz_editor_view_init(SubGhzEditorApp* app) {
    variable_item_list_set_enter_callback(app->var_list, subghz_editor_list_enter, app);
    subghz_editor_view_refresh_list(app);
}

void subghz_editor_show_about(SubGhzEditorApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#Sub-GHz Editor v1.1\e#\n"
        "\e#Frequency & Key Editor\e#\n"
        "-------------------\n"
        "Features:\n"
        "- Frequency Presets\n"
        "- Manual CC1101 Input\n"
        "- Hex Key Code Editor\n"
        "- Direct .sub File Load\n"
        "- Save & Save As Copy\n"
        "-------------------\n"
        "Author:\n"
        "Vazgen Grigoryan (VG)\n"
        "License: MIT\n"
        "-------------------\n"
        "GitHub Project:\n"
        "vazgen3201/flipper-\n"
        "subghz-editor\n");
    view_dispatcher_switch_to_view(app->view_dispatcher, SubGhzEditorViewWidget);
}
