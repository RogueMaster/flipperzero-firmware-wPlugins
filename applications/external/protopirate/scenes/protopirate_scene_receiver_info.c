// scenes/protopirate_scene_receiver_info.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "../helpers/protopirate_bruteforce_host.h"
#include "../protocols/protocol_items.h"
#include "proto_pirate_icons.h"
#include <storage/storage.h>

#define TAG "PPReceiverInfo"

#define STATE_EMULATE 0
#define STATE_BF      1

static void protopirate_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static void protopirate_scene_receiver_info_text_input_callback(void* context) {
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSaveConfirm);
}

static void protopirate_receiver_info_build_normal_widget(ProtoPirateApp* app) {
    widget_reset(app->widget);
    app->emulate_disabled_for_loaded = true;

    FuriString* text = furi_string_alloc();
    protopirate_history_get_text_item_menu(app->txrx->history, text, app->txrx->idx_menu_chosen);
    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));

    furi_string_reset(text);
    protopirate_history_get_text_item_detail(
        app->txrx->history, app->txrx->idx_menu_chosen, text, app->txrx->environment);

    bool is_psa = false;
    bool offers_bf = false;
    FlipperFormat* ff =
        protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(ff) {
        FuriString* protocol = furi_string_alloc();
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
            const char* protocol_name = furi_string_get_cstr(protocol);
            const char* canonical = protopirate_protocol_catalog_canonical_name(protocol_name);
            if(strcmp(canonical, "PSA") == 0) is_psa = true;
            offers_bf = protopirate_protocol_catalog_offers_bruteforce(protocol_name);
            app->emulate_disabled_for_loaded = !protopirate_protocol_catalog_can_tx(protocol_name);
        }
        furi_string_free(protocol);
    }

    const char* text_str = furi_string_get_cstr(text);
    const char* first_newline = strchr(text_str, '\r');
    if(first_newline) {
        text_str = first_newline + 1;
        if(*text_str == '\n') text_str++;
    } else {
        first_newline = strchr(text_str, '\n');
        if(first_newline) text_str = first_newline + 1;
    }

    if(is_psa) {
        FuriString* reformatted = furi_string_alloc();
        const char* current = text_str;
        while(*current) {
            const char* line_end = strchr(current, '\r');
            if(!line_end) line_end = strchr(current, '\n');
            if(!line_end) line_end = current + strlen(current);

            if(strncmp(current, "Ser:", 4) == 0) {
                size_t ser_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s", (int)ser_len, current);
                const char* next_line = line_end;
                if(*next_line == '\r') next_line++;
                if(*next_line == '\n') next_line++;
                if(strncmp(next_line, "Cnt:", 4) == 0) {
                    const char* cnt_end = strchr(next_line, '\r');
                    if(!cnt_end) cnt_end = strchr(next_line, '\n');
                    if(!cnt_end) cnt_end = next_line + strlen(next_line);
                    furi_string_cat_printf(
                        reformatted, " %.*s\r\n", (int)(cnt_end - next_line), next_line);
                    current = cnt_end;
                } else {
                    furi_string_cat_printf(reformatted, "\r\n");
                    current = line_end;
                }
                if(*current == '\r') current++;
                if(*current == '\n') current++;
            } else {
                size_t line_len = line_end - current;
                furi_string_cat_printf(reformatted, "%.*s\r\n", (int)line_len, current);
                current = line_end;
                if(*current == '\r') current++;
                if(*current == '\n') current++;
            }
            if(*current == '\0') break;
        }
        widget_add_string_multiline_element(
            app->widget,
            0,
            11,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(reformatted));
        furi_string_free(reformatted);
    } else {
        widget_add_string_multiline_element(
            app->widget, 0, 11, AlignLeft, AlignTop, FontSecondary, text_str);
    }

    bool needs_bf = false;
    bool error = false;
    if(offers_bf && protopirate_bruteforce_plugin_ensure_loaded(app) &&
       app->running_bruteforce_plugin.bruteforce_plugin) {
        needs_bf = app->running_bruteforce_plugin.bruteforce_plugin->widget_left_should_bruteforce(
            app, ff);
    } else if(offers_bf) {
        //Show the user the error in the button.
        widget_add_button_element(app->widget, GuiButtonTypeLeft, "(Error)", NULL, app);
        needs_bf = false;
        error = true;
    }

    protopirate_bruteforce_plugin_unload_if_idle(app);
    if(needs_bf) {
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneReceiverInfo, STATE_BF);
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "BF",
            protopirate_scene_receiver_info_widget_callback,
            app);
    } else if(!error) {
        scene_manager_set_scene_state(
            app->scene_manager, ProtoPirateSceneReceiverInfo, STATE_EMULATE);

#ifdef ENABLE_EMULATE_FEATURE
        if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
            widget_add_button_element(
                app->widget,
                GuiButtonTypeLeft,
                "Emulate",
                protopirate_scene_receiver_info_widget_callback,
                app);
        }
#endif
    }

    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        protopirate_history_has_matched_saved(app->txrx->history, app->txrx->idx_menu_chosen) ?
            "Update" :
            "Save",
        protopirate_scene_receiver_info_widget_callback,
        app);

    furi_string_free(text);
}

void protopirate_receiver_info_rebuild_normal_widget(ProtoPirateApp* app) {
    protopirate_receiver_info_build_normal_widget(app);
}

void protopirate_saved_info_rebuild_normal_widget(void* app) {
    protopirate_scene_saved_info_on_enter((ProtoPirateApp*)app);
}

static void protopirate_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;
    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeRight) {
            bool has_match = protopirate_history_has_matched_saved(
                app->txrx->history, app->txrx->idx_menu_chosen);
            view_dispatcher_send_custom_event(
                app->view_dispatcher,
                has_match ? ProtoPirateCustomEventReceiverInfoUpdate :
                            ProtoPirateCustomEventReceiverInfoSave);
        } else if(result == GuiButtonTypeLeft) {
            if(scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneReceiverInfo) ==
               STATE_BF) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventBruteforceStart);

            }
#ifdef ENABLE_EMULATE_FEATURE
            else if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventReceiverInfoEmulate);
            }
#endif
        } else if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventBruteforceComplete);
        }
    }
}

void protopirate_scene_receiver_info_on_enter(void* context) {
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->emulate_disabled_for_loaded = false;

    if(app->running_bruteforce_plugin.bruteforce_plugin) {
        if(app->running_bruteforce_plugin.bruteforce_plugin->is_running(app)) {
            app->running_bruteforce_plugin.bruteforce_plugin->on_scene_enter(
                app, ProtoPirateBruteForceContextReceiverInfo);
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            return;
        }
    }

    protopirate_receiver_info_build_normal_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventBruteforceStart) {
            if(protopirate_bruteforce_plugin_ensure_loaded(app) &&

               app->running_bruteforce_plugin.bruteforce_plugin) {
                FURI_LOG_E(TAG, "Started Bruteforce");
                consumed = app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
                    app, ProtoPirateBruteForceContextReceiverInfo, event);
            } else {
                FURI_LOG_E(TAG, "Failed to load PSA bruteforce plugin");
                notification_message(app->notifications, &sequence_error);
                consumed = true;
            }
            return consumed;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->running_bruteforce_plugin.bruteforce_plugin &&
           app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
               app, ProtoPirateBruteForceContextReceiverInfo, event)) {
            consumed = true;
        } else if(app->dialog_showing) {
            app->dialog_showing = false;
            consumed = false;
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
        } else {
            consumed = false;
        }
        return consumed;
    } else if(app->running_bruteforce_plugin.bruteforce_plugin) {
        FURI_LOG_E(TAG, "Bruteforcing");
        return app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
            app, ProtoPirateBruteForceContextReceiverInfo, event);
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventReceiverInfoUpdate) {
            uint16_t idx = app->txrx->idx_menu_chosen;
            const char* saved_path =
                protopirate_history_get_matched_saved_path(app->txrx->history, idx);
            if(saved_path) {
                FlipperFormat* rx_ff = protopirate_history_get_raw_data(app->txrx->history, idx);
                if(rx_ff) {
                    if(protopirate_storage_save_capture_to_path(rx_ff, saved_path)) {
                        notification_message(app->notifications, &sequence_success);
                        FURI_LOG_I(
                            TAG, "Updated saved capture from received signal: %s", saved_path);
                    } else {
                        notification_message(app->notifications, &sequence_error);
                        FURI_LOG_E(TAG, "Failed to update saved capture: %s", saved_path);
                    }
                    protopirate_history_release_scratch(app->txrx->history);
                } else {
                    notification_message(app->notifications, &sequence_error);
                    FURI_LOG_E(TAG, "No received capture available for update");
                }
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            consumed = true;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoSave) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
            if(ff) {
#define YMD_LENGTH 17
                char* file_name_str = malloc(PROTOPIRATE_PROTOCOL_NAME_MAX + YMD_LENGTH);
                FURI_LOG_D(TAG, "Save called:");

                if(app->datetime_filenames) {
                    //Get the date and time to save.
                    DateTime date_time;
                    furi_hal_rtc_get_datetime(&date_time);
                    snprintf(
                        file_name_str,
                        50,
                        "%.2d%.2d%.2d_%.2d.%.2d.%.2d_",
                        date_time.year,
                        date_time.month,
                        date_time.day,
                        date_time.hour,
                        date_time.minute,
                        date_time.second);
                }

                // Extract protocol name
                FuriString* buffer = furi_string_alloc();
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, "Protocol", buffer)) {
                    furi_string_set_str(buffer, "Unknown");
                }
                //Add the protocol
                char* file_name_dup = strdup(file_name_str);
                snprintf(file_name_str, 50, "%s%s", file_name_dup, furi_string_get_cstr(buffer));
                free(file_name_dup);
                furi_string_reset(buffer);

                // Clean protocol name for filename
                for(char* p = file_name_str; *p; p++) {
                    if(*p == '/' || *p == ' ') *p = '_';
                }

                // Get the next auto-generated filename (just the name part)
                protopirate_ensure_text_input(app);
                if(protopirate_storage_get_next_filename(
                       file_name_str, buffer, app->datetime_filenames)) {
                    // Extract just the filename without folder and extension
                    FURI_LOG_D(TAG, "Filename Made up: %s ", furi_string_get_cstr(buffer));

                    const char* full = furi_string_get_cstr(buffer);
                    const char* slash = strrchr(full, '/');
                    const char* name_start = slash ? slash + 1 : full;

                    // Copy without extension
                    size_t name_len = strlen(name_start);
                    const char* dot = strrchr(name_start, '.');
                    if(dot) name_len = dot - name_start;
                    if(name_len >= 64) name_len = 64;

                    if(app->save_filename) free(app->save_filename);
                    app->save_filename = malloc(name_len + 1);
                    memcpy(app->save_filename, name_start, name_len);
                } else {
                    if(app->save_filename) free(app->save_filename);
                    uint8_t len = 8;
                    app->save_filename = malloc(len);
                    snprintf(app->save_filename, len, "capture");
                }

                // Store context for when text input confirms
                app->save_history_idx = app->txrx->idx_menu_chosen;

                // Configure and show text input
                text_input_set_header_text(app->text_input, "Save filename:");
                text_input_set_result_callback(
                    app->text_input,
                    protopirate_scene_receiver_info_text_input_callback,
                    app,
                    app->save_filename,
                    strlen(app->save_filename),
                    false); // don't clear default text

                view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewTextInput);
                app->dialog_showing = true;
                free(file_name_str);
                furi_string_free(buffer);
            }
            consumed = true;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoSaveConfirm) {
            // User confirmed the filename in text input
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->save_history_idx);
            if(ff) {
                // Build full path: folder/filename.psf
                FuriString* save_path = furi_string_alloc_printf(
                    "%s/%s%s",
                    PROTOPIRATE_APP_FOLDER,
                    app->save_filename,
                    PROTOPIRATE_APP_EXTENSION);

                if(protopirate_storage_save_capture_to_path(ff, furi_string_get_cstr(save_path))) {
                    notification_message(app->notifications, &sequence_success);
                    FURI_LOG_I(TAG, "Saved to: %s", furi_string_get_cstr(save_path));
                } else {
                    notification_message(app->notifications, &sequence_error);
                    FURI_LOG_E(TAG, "Save failed");
                }
                furi_string_free(save_path);
            }

            // Return to the receiver info widget
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            app->dialog_showing = false;

            //Kill the text_input view.
            protopirate_free_text_input(app);
            consumed = true;
        }

#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == ProtoPirateCustomEventReceiverInfoEmulate &&
           app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
            FuriString* hist_path = furi_string_alloc();
            if(protopirate_history_get_capture_path(
                   app->txrx->history, app->txrx->idx_menu_chosen, hist_path)) {
                protopirate_history_release_scratch(app->txrx->history);
                size_t len = furi_string_utf8_length(hist_path) + 1;
                if(app->loaded_file_path) free(app->loaded_file_path);
                app->loaded_file_path = malloc(len);
                snprintf(app->loaded_file_path, len, furi_string_get_cstr(hist_path));
                furi_string_free(hist_path);
                FURI_LOG_I(TAG, "Emulate from history file: %s", app->loaded_file_path);
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
            } else {
                furi_string_free(hist_path);
                FURI_LOG_E(TAG, "No capture path for index %d", app->txrx->idx_menu_chosen);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
        if(event.event == ProtoPirateCustomEventBruteforceComplete) {
            protopirate_scene_receiver_info_on_enter(app);
            consumed = true;
        }
#endif
    }

    return consumed;
}

void protopirate_scene_receiver_info_on_exit(void* context) {
    ProtoPirateApp* app = context;
    protopirate_bruteforce_context_release(app);
    widget_reset(app->widget);
    protopirate_free_text_input(app);
    if(app->txrx && app->txrx->history) {
        protopirate_history_release_scratch(app->txrx->history);
    }
}
