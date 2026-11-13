#include "keytool_i.h"
#include "keytool_mifare.h"
#include "keytool_dict.h"
#include "keytool_log.h"
#include "keytool_cache.h"
#include <stream/stream.h>
#include <stream/file_stream.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------
// Navigation helpers
// ---------------------------------------------------------------------

static uint32_t keytool_back_to_submenu(void* context) {
    UNUSED(context);
    return KeyToolViewSubmenu;
}

// The log-detail TextBox can be reached either straight after a scan
// (-> back to main menu) or from the saved-logs list (-> back to that
// list); app->text_box_return_view tracks which.
// NOTE: the TextBox module uses its own view context internally (it is the
// TextBox*), so we must not override it. The back callback receives that
// context, hence the app is reached through this pointer instead.
static KeyToolApp* keytool_app_instance = NULL;

static uint32_t keytool_text_box_back_callback(void* context) {
    UNUSED(context);
    return keytool_app_instance->text_box_return_view;
}

static bool keytool_navigation_exit_callback(void* context) {
    KeyToolApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

// ---------------------------------------------------------------------
// About scene
// ---------------------------------------------------------------------

static void keytool_about_build(KeyToolApp* app) {
    widget_reset(app->widget_about);
    widget_add_string_element(
        app->widget_about, 64, 24, AlignCenter, AlignCenter, FontPrimary, "v1.0");
    widget_add_string_element(
        app->widget_about, 64, 40, AlignCenter, AlignCenter, FontSecondary, "created by Marcopiiit");
}

// ---------------------------------------------------------------------
// Start / scan flow
// ---------------------------------------------------------------------

static void keytool_scene_start_run(KeyToolApp* app) {
    FuriString* dump_path = furi_string_alloc_set_str(KEYTOOL_DEFAULT_DUMP_FOLDER);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);
    browser_options.base_path = KEYTOOL_DEFAULT_DUMP_FOLDER;

    bool picked = dialog_file_browser_show(app->dialogs, dump_path, dump_path, &browser_options);
    if(!picked) {
        furi_string_free(dump_path);
        return;
    }
    furi_string_set(app->dump_file_path, dump_path);

    // Card name = dump's file name (without extension).
    FuriString* card_name = furi_string_alloc();
    {
        const char* dump_cstr = furi_string_get_cstr(dump_path);
        const char* base = strrchr(dump_cstr, '/');
        base = base ? base + 1 : dump_cstr;
        furi_string_set_str(card_name, base);
        size_t dot = furi_string_search_rchar(card_name, '.');
        if(dot != FURI_STRING_FAILURE) furi_string_left(card_name, dot);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewLoading);

    KeyToolKeyEntry* keys = malloc(sizeof(KeyToolKeyEntry) * KEYTOOL_MAX_KEYS);
    size_t key_count = 0;
    FuriString* err = furi_string_alloc();
    bool parse_ok =
        keytool_mifare_parse_dump(app->storage, furi_string_get_cstr(dump_path), keys, &key_count, err);

    size_t found_count = 0;

    if(!parse_ok || key_count == 0) {
        furi_string_printf(
            app->log_text, "Could not extract keys from dump.\n%s", furi_string_get_cstr(err));
    } else {
        FuriString** dict_paths = malloc(sizeof(FuriString*) * KEYTOOL_MAX_DICTS);
        FuriString** dict_names = malloc(sizeof(FuriString*) * KEYTOOL_MAX_DICTS);
        for(size_t i = 0; i < KEYTOOL_MAX_DICTS; i++) {
            dict_paths[i] = furi_string_alloc();
            dict_names[i] = furi_string_alloc();
        }
        size_t dict_count = 0;
        keytool_dict_autodetect(
            app->storage, dict_paths, dict_names, &dict_count, KEYTOOL_MAX_DICTS);

        if(dict_count == 0) {
            furi_string_set_str(
                app->log_text,
                "No dictionaries found automatically.\n\n"
                "Expected one of:\n"
                "- nfc/assets/mf_classic_dict\n"
                "  (.nfc / _user.nfc)\n"
                "- nfc/dict/*.dict|.txt|.nfc\n"
                "- /ext/dict/*.dict|.txt|.nfc");
        } else {
            bool* found_matrix = malloc(sizeof(bool) * key_count * dict_count);
            memset(found_matrix, 0, sizeof(bool) * key_count * dict_count);

            for(size_t d = 0; d < dict_count; d++) {
                keytool_dict_scan_file(
                    app->storage,
                    furi_string_get_cstr(dict_paths[d]),
                    keys,
                    key_count,
                    found_matrix,
                    dict_count,
                    d);
            }

            // Remember the keys that are in no dictionary (Cache menu).
            keytool_cache_save(
                app->storage,
                furi_string_get_cstr(card_name),
                keys,
                key_count,
                found_matrix,
                dict_count);

            // Build two sections so that keys NOT found in any
            // dictionary are shown first, as requested.
            FuriString* not_found_section = furi_string_alloc();
            FuriString* found_section = furi_string_alloc();
            size_t not_found_count = 0;

            for(size_t k = 0; k < key_count; k++) {
                FuriString* key_line = furi_string_alloc_printf(
                    "S%lu Key%c ", (unsigned long)keys[k].sector, keys[k].type);
                for(int b = 0; b < 6; b++)
                    furi_string_cat_printf(key_line, "%02X", keys[k].bytes[b]);

                bool any_found = false;
                FuriString* dicts_line = furi_string_alloc();
                for(size_t d = 0; d < dict_count; d++) {
                    if(found_matrix[k * dict_count + d]) {
                        if(any_found) furi_string_cat_str(dicts_line, ", ");
                        furi_string_cat(dicts_line, dict_names[d]);
                        any_found = true;
                    }
                }

                if(any_found) {
                    furi_string_cat_printf(
                        found_section,
                        "%s\n  FOUND in: %s\n",
                        furi_string_get_cstr(key_line),
                        furi_string_get_cstr(dicts_line));
                    found_count++;
                } else {
                    furi_string_cat_printf(
                        not_found_section, "%s\n  NOT FOUND\n", furi_string_get_cstr(key_line));
                    not_found_count++;
                }

                furi_string_free(dicts_line);
                furi_string_free(key_line);
            }

            furi_string_reset(app->log_text);
            furi_string_cat_printf(
                app->log_text,
                "Dump: %s\nKeys: %zu   Dicts: %zu\n\n",
                furi_string_get_cstr(dump_path),
                key_count,
                dict_count);

            if(not_found_count > 0) {
                furi_string_cat_printf(
                    app->log_text, "=== NOT FOUND (%zu) ===\n", not_found_count);
                furi_string_cat(app->log_text, not_found_section);
                furi_string_cat_str(app->log_text, "\n");
            }
            if(found_count > 0) {
                furi_string_cat_printf(app->log_text, "=== FOUND (%zu) ===\n", found_count);
                furi_string_cat(app->log_text, found_section);
            }
            furi_string_cat_printf(
                app->log_text, "\nSummary: %zu/%zu keys found\n", found_count, key_count);

            furi_string_free(not_found_section);
            furi_string_free(found_section);
            free(found_matrix);
        }

        for(size_t i = 0; i < KEYTOOL_MAX_DICTS; i++) {
            furi_string_free(dict_paths[i]);
            furi_string_free(dict_names[i]);
        }
        free(dict_paths);
        free(dict_names);
    }

    furi_string_free(err);
    free(keys);

    // Auto-save this scan as a log entry (date = current RTC time).

    FuriString* date_display = furi_string_alloc();
    FuriString* date_prefix = furi_string_alloc();
    keytool_log_datetime_now(date_display, date_prefix);

    keytool_log_save_entry(
        app->storage,
        furi_string_get_cstr(card_name),
        furi_string_get_cstr(date_display),
        furi_string_get_cstr(date_prefix),
        app->log_text);

    furi_string_free(card_name);
    furi_string_free(date_display);
    furi_string_free(date_prefix);
    furi_string_free(dump_path);

    app->text_box_return_view = KeyToolViewSubmenu;
    text_box_set_text(app->text_box, furi_string_get_cstr(app->log_text));
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewLogDetail);
}

// ---------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------

static void keytool_submenu_callback(void* context, uint32_t index) {
    KeyToolApp* app = context;
    switch(index) {
    case KeyToolSubmenuIndexStart:
        keytool_scene_start_run(app);
        break;
    case KeyToolSubmenuIndexLog:
        keytool_log_list_refresh(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewLogList);
        break;
    case KeyToolSubmenuIndexCache:
        keytool_cache_list_refresh(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewCacheList);
        break;
    case KeyToolSubmenuIndexAbout:
        keytool_about_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewAbout);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------
// App alloc / free
// ---------------------------------------------------------------------

static KeyToolApp* keytool_app_alloc(void) {
    KeyToolApp* app = malloc(sizeof(KeyToolApp));
    keytool_app_instance = app;

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->gui = furi_record_open(RECORD_GUI);

    app->dump_file_path = furi_string_alloc();
    app->log_text = furi_string_alloc_set_str("No scan has been run yet.");
    app->text_box_return_view = KeyToolViewSubmenu;

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, keytool_navigation_exit_callback);

    // Main menu
    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Start", KeyToolSubmenuIndexStart, keytool_submenu_callback, app);
    submenu_add_item(app->submenu, "Log", KeyToolSubmenuIndexLog, keytool_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Cache", KeyToolSubmenuIndexCache, keytool_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", KeyToolSubmenuIndexAbout, keytool_submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyToolViewSubmenu, submenu_get_view(app->submenu));

    // Saved logs list (custom view: card name + date, OK opens, LEFT deletes)
    app->log_list_view = keytool_log_list_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, KeyToolViewLogList, app->log_list_view);

    // Cache list (missing keys grouped by dump; hold OK imports them)
    app->cache_view = keytool_cache_list_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, KeyToolViewCacheList, app->cache_view);

    // Log detail (one scan's full result)
    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    view_set_previous_callback(text_box_get_view(app->text_box), keytool_text_box_back_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyToolViewLogDetail, text_box_get_view(app->text_box));

    // About
    app->widget_about = widget_alloc();
    view_set_previous_callback(widget_get_view(app->widget_about), keytool_back_to_submenu);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyToolViewAbout, widget_get_view(app->widget_about));

    // Loading / scanning spinner
    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KeyToolViewLoading, loading_get_view(app->loading));

    return app;
}

static void keytool_app_free(KeyToolApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewLogList);
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewCacheList);
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewLogDetail);
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, KeyToolViewLoading);

    submenu_free(app->submenu);
    keytool_log_list_free(app->log_list_view);
    keytool_cache_list_free(app->cache_view);
    text_box_free(app->text_box);
    widget_free(app->widget_about);
    loading_free(app->loading);

    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->dump_file_path);
    furi_string_free(app->log_text);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    keytool_app_instance = NULL;
    free(app);
}

// ---------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------

int32_t keytool_app(void* p) {
    UNUSED(p);
    KeyToolApp* app = keytool_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyToolViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    keytool_app_free(app);
    return 0;
}
