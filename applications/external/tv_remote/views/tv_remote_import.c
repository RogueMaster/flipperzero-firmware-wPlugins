/**
 * @file views/tv_remote_import.c
 * @brief "Import from saved IR file" flow using only Submenu views.
 *
 * Flow:
 *   1. User picks an .ir file from a Submenu list.
 *   2. User sees a Submenu of TV buttons. Each row shows the button name and the
 *      source signal currently mapped to it (or "-").
 *   3. Selecting a button opens a Submenu of source signal names.
 *   4. Bottom rows: "Auto-map names" and "Save imported remote".
 */

#include "tv_remote_import.h"
#include "../flipper_tv_remote.h"

#include <gui/elements.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations. */
static void tv_remote_import_button_select_callback(void* context, uint32_t index);
static void tv_remote_import_signal_select_callback(void* context, uint32_t index);
static void tv_remote_import_build_signal_submenu(TvRemoteApp* app);
static void tv_remote_import_text_input_callback(void* context);

/* Friendly labels for the mapping rows. */
static const char* const import_button_labels[TV_BUTTON_COUNT] = {
    "Power",
    "Mute",
    "Vol Up",
    "Vol Dn",
    "Ch Up",
    "Ch Dn",
    "Up",
    "Down",
    "Left",
    "Right",
    "OK",
    "Back",
    "Home",
    "Play/Pause",
};

/* Indices in the button-select submenu after the real TV buttons. */
#define IMPORT_BTN_AUTOMAP (TV_BUTTON_COUNT)
#define IMPORT_BTN_SAVE    (TV_BUTTON_COUNT + 1)

/* Auto-mapping aliases. First matching source name wins for a given button. */
static const struct {
    TvButton button;
    const char* aliases[5];
} auto_map_rules[] = {
    {TvButtonPower, {"power", "pwr", "onoff", "on/off", NULL}},
    {TvButtonMute, {"mute", "silence", NULL, NULL, NULL}},
    {TvButtonVolUp, {"vol_up", "vol+", "volume_up", "vol up", NULL}},
    {TvButtonVolDn, {"vol_dn", "vol-", "volume_down", "vol down", NULL}},
    {TvButtonChUp, {"ch_up", "ch+", "channel_up", "channel up", NULL}},
    {TvButtonChDn, {"ch_dn", "ch-", "channel_down", "channel down", NULL}},
    {TvButtonUp, {"up", NULL, NULL, NULL, NULL}},
    {TvButtonDown, {"down", NULL, NULL, NULL, NULL}},
    {TvButtonLeft, {"left", NULL, NULL, NULL, NULL}},
    {TvButtonRight, {"right", NULL, NULL, NULL, NULL}},
    {TvButtonOk, {"ok", "enter", "select", NULL, NULL}},
    {TvButtonBack, {"back", "return", "exit", NULL, NULL}},
    {TvButtonHome, {"home", "menu", "guide", NULL, NULL}},
    {TvButtonPlayPause, {"play_pause", "play", "pause", "play/pause", NULL}},
};

/* ---- Mapping label helper ---- */

static const char* tv_remote_import_get_mapping_label(TvRemoteApp* app, size_t index) {
    if(index >= TV_BUTTON_COUNT) return "";
    if(!app->import.source_names) return "-";
    int16_t src = app->import.map[index];
    if(src < 0 || (size_t)src >= app->import.source_count) {
        return "-";
    }
    return app->import.source_names[(size_t)src];
}

/* ---- Auto-map by name ---- */

static void tv_remote_import_auto_map(TvRemoteApp* app) {
    if(app->import.source_count == 0) return;

    for(size_t b = 0; b < TV_BUTTON_COUNT; b++) {
        app->import.map[b] = -1;
    }

    for(size_t b = 0; b < TV_BUTTON_COUNT; b++) {
        for(size_t a = 0; a < COUNT_OF(auto_map_rules); a++) {
            if(auto_map_rules[a].button != (TvButton)b) continue;
            for(size_t s = 0; s < app->import.source_count; s++) {
                if(app->import.map[b] >= 0) break;
                const char* src = app->import.source_names[s];
                for(size_t alias_idx = 0; alias_idx < COUNT_OF(auto_map_rules[a].aliases);
                    alias_idx++) {
                    const char* alias = auto_map_rules[a].aliases[alias_idx];
                    if(!alias) break;
                    if(strcasecmp(src, alias) == 0) {
                        app->import.map[b] = (int16_t)s;
                        break;
                    }
                }
            }
        }
    }
}

/* ---- IR file scanner / picker ---- */

bool tv_remote_import_run_file_browser(TvRemoteApp* app) {
    if(!app) return false;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    FuriString* start_path = furi_string_alloc_set_str(EXT_PATH("infrared"));
    FuriString* result_path = furi_string_alloc();

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".ir", NULL);
    options.base_path = EXT_PATH("infrared");

    bool chosen = dialog_file_browser_show(dialogs, result_path, start_path, &options);

    furi_string_free(start_path);
    furi_record_close(RECORD_DIALOGS);

    if(!chosen) {
        furi_string_free(result_path);
        return false;
    }

    tv_remote_import_reset(&app->import);
    strncpy(
        app->import.source_path,
        furi_string_get_cstr(result_path),
        sizeof(app->import.source_path) - 1);
    app->import.source_path[sizeof(app->import.source_path) - 1] = '\0';
    furi_string_free(result_path);

    FURI_LOG_I(TV_REMOTE_APP_TAG, "Import selected: %s", app->import.source_path);

    if(!tv_remote_list_ir_file_signals(
           app->import.source_path, &app->import.source_names, &app->import.source_count)) {
        tv_remote_import_reset(&app->import);
        FURI_LOG_E(TV_REMOTE_APP_TAG, "Failed to parse IR file");
        notification_message(app->notifications, &sequence_error);
        return false;
    }

    if(app->import.source_count == 0) {
        tv_remote_import_reset(&app->import);
        FURI_LOG_E(TV_REMOTE_APP_TAG, "No signals found in IR file");
        notification_message(app->notifications, &sequence_error);
        return false;
    }

    FURI_LOG_I(TV_REMOTE_APP_TAG, "Found %zu signals", app->import.source_count);
    tv_remote_import_auto_map(app);
    tv_remote_import_build_button_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewImportButtonSelect);
    return true;
}

/* ---- Button selection submenu ---- */

static char button_row_label[TV_BUTTON_COUNT][TV_REMOTE_SOURCE_NAME_MAX + 16];

void tv_remote_import_build_button_submenu(TvRemoteApp* app) {
    if(!app || !app->import_button_submenu) return;
    submenu_reset(app->import_button_submenu);
    submenu_set_header(app->import_button_submenu, "Map buttons");

    for(size_t i = 0; i < TV_BUTTON_COUNT; i++) {
        snprintf(
            button_row_label[i],
            sizeof(button_row_label[i]),
            "%s  >  %s",
            import_button_labels[i],
            tv_remote_import_get_mapping_label(app, i));
        submenu_add_item(
            app->import_button_submenu,
            button_row_label[i],
            (uint32_t)i,
            tv_remote_import_button_select_callback,
            app);
    }

    submenu_add_item(
        app->import_button_submenu,
        "Auto-map names",
        IMPORT_BTN_AUTOMAP,
        tv_remote_import_button_select_callback,
        app);

    submenu_add_item(
        app->import_button_submenu,
        "Save imported remote",
        IMPORT_BTN_SAVE,
        tv_remote_import_button_select_callback,
        app);
}

static void tv_remote_import_button_select_callback(void* context, uint32_t index) {
    TvRemoteApp* app = context;
    if(!app) return;

    if(index < TV_BUTTON_COUNT) {
        app->import.selected_button = (uint8_t)index;
        tv_remote_import_build_signal_submenu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewImportSignalSelect);
        return;
    }

    if(index == IMPORT_BTN_AUTOMAP) {
        tv_remote_import_auto_map(app);
        tv_remote_import_build_button_submenu(app);
        submenu_set_selected_item(app->import_button_submenu, IMPORT_BTN_AUTOMAP);
        return;
    }

    if(index == IMPORT_BTN_SAVE) {
        app->text_input_buf[0] = '\0';
        text_input_set_result_callback(
            app->text_input,
            tv_remote_import_text_input_callback,
            app,
            app->text_input_buf,
            TV_REMOTE_NAME_MAX,
            true);
        text_input_set_header_text(app->text_input, "Save imported remote:");
        View* text_input_view = text_input_get_view(app->text_input);
        view_set_previous_callback(text_input_view, tv_remote_back_to_import_callback);
        view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewTextInput);
        return;
    }
}

/* ---- Signal selection submenu ---- */

static void tv_remote_import_signal_select_callback(void* context, uint32_t index) {
    TvRemoteApp* app = context;
    if(!app) return;
    if(index == 0) {
        /* "-" unmaps the button. */
        app->import.map[app->import.selected_button] = -1;
    } else if(index - 1 < app->import.source_count) {
        app->import.map[app->import.selected_button] = (int16_t)(index - 1);
    }

    tv_remote_import_build_button_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewImportButtonSelect);
}

static void tv_remote_import_build_signal_submenu(TvRemoteApp* app) {
    if(!app || !app->import_signal_submenu) return;
    submenu_reset(app->import_signal_submenu);
    submenu_set_header(
        app->import_signal_submenu, import_button_labels[app->import.selected_button]);
    submenu_add_item(
        app->import_signal_submenu, "-", 0, tv_remote_import_signal_select_callback, app);
    for(size_t i = 0; i < app->import.source_count; i++) {
        submenu_add_item(
            app->import_signal_submenu,
            app->import.source_names[i],
            (uint32_t)(i + 1),
            tv_remote_import_signal_select_callback,
            app);
    }
}

/* ---- Text input callback for import save ---- */

static void tv_remote_import_text_input_callback(void* context) {
    TvRemoteApp* app = context;
    if(!app) return;

    char* src = app->text_input_buf;
    char* dst = app->import.new_remote_name;
    size_t out_len = 0;
    for(; *src && out_len < TV_REMOTE_NAME_MAX; src++) {
        char c = *src;
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_') {
            *dst++ = c;
            out_len++;
        } else if(c == ' ') {
            *dst++ = '_';
            out_len++;
        }
    }
    *dst = '\0';

    if(out_len == 0) {
        view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewImportButtonSelect);
        return;
    }

    if(tv_remote_import_save_mapped_remote(app, app->import.new_remote_name)) {
        notification_message(app->notifications, &sequence_success);
        tv_remote_import_reset(&app->import);
        view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewMainMenu);
    } else {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_switch_to_view(app->view_dispatcher, TvRemoteViewImportButtonSelect);
    }
}

/* ---- Public API ---- */

void tv_remote_import_view_alloc(TvRemoteApp* app) {
    if(!app) return;

    app->import_button_submenu = submenu_alloc();
    app->import_signal_submenu = submenu_alloc();
}

void tv_remote_import_view_free(TvRemoteApp* app) {
    if(!app) return;
    submenu_free(app->import_button_submenu);
    submenu_free(app->import_signal_submenu);
}
