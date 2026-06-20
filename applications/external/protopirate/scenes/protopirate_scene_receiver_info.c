// scenes/protopirate_scene_receiver_info.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "../helpers/protopirate_psa_bf_host.h"
#include "proto_pirate_icons.h"

#define TAG "ProtoPirateReceiverInfo"

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

    FuriString* text = furi_string_alloc();
    protopirate_history_get_text_item_menu(app->txrx->history, text, app->txrx->idx_menu_chosen);
    widget_add_string_element(
        app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(text));

    furi_string_reset(text);
    protopirate_history_get_text_item_detail(
        app->txrx->history, app->txrx->idx_menu_chosen, text, app->txrx->environment);

    bool is_psa = false;
    FlipperFormat* ff =
        protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(ff) {
        FuriString* protocol = furi_string_alloc();
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
            if(furi_string_cmp_str(protocol, "PSA") == 0) is_psa = true;
            app->emulate_disabled_for_loaded = (furi_string_cmp_str(protocol, "Scher-Khan") == 0);
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

    bool psa_needs_bf = false;
    if(is_psa && protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        psa_needs_bf = app->psa_bf_plugin->widget_left_should_bruteforce(
            app, ProtoPiratePsaBfContextReceiverInfo);
    }
    if(psa_needs_bf) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Brute force",
            protopirate_scene_receiver_info_widget_callback,
            app);
    } else
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

    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        "Save",
        protopirate_scene_receiver_info_widget_callback,
        app);

    furi_string_free(text);
}

void protopirate_receiver_info_rebuild_normal_widget(void* app) {
    protopirate_receiver_info_build_normal_widget((ProtoPirateApp*)app);
}

static void protopirate_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;
    if(type == InputTypeShort || type == InputTypeLong) {
        if(result == GuiButtonTypeRight) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSave);
        } else if(result == GuiButtonTypeLeft) {
            if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin &&
               app->psa_bf_plugin->widget_left_should_bruteforce(
                   app, ProtoPiratePsaBfContextReceiverInfo)) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventReceiverInfoBruteforceStart);
            }
#ifdef ENABLE_EMULATE_FEATURE
            else if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventReceiverInfoEmulate);
            }
#endif
        } else if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventReceiverInfoBruteforceCancel);
        }
    }
}

void protopirate_scene_receiver_info_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_widget(app) || !protopirate_ensure_text_input(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->emulate_disabled_for_loaded = false;

    if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        if(app->psa_bf_plugin->is_running(app)) {
            app->psa_bf_plugin->on_scene_enter(app, ProtoPiratePsaBfContextReceiverInfo);
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

    if(protopirate_psa_bf_plugin_ensure_loaded(app) && app->psa_bf_plugin) {
        if(app->psa_bf_plugin->is_running(app) ||
           event.event == ProtoPirateCustomEventPsaBruteforceComplete ||
           event.event == ProtoPirateCustomEventReceiverInfoBruteforceStart ||
           event.event == ProtoPirateCustomEventReceiverInfoBruteforceCancel) {
            consumed = app->psa_bf_plugin->on_scene_event(
                app, ProtoPiratePsaBfContextReceiverInfo, event);
            if(consumed) return true;
        }
        if(event.type == SceneManagerEventTypeBack &&
           app->psa_bf_plugin->on_scene_event(app, ProtoPiratePsaBfContextReceiverInfo, event)) {
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventReceiverInfoSave) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
            if(ff) {
                FuriString* protocol = furi_string_alloc();
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, FF_PROTOCOL, protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                furi_string_replace_all(protocol, "/", "_");
                furi_string_replace_all(protocol, " ", "_");

                FuriString* auto_path = furi_string_alloc();
                if(protopirate_storage_get_next_filename(
                       furi_string_get_cstr(protocol), auto_path)) {
                    const char* full = furi_string_get_cstr(auto_path);
                    const char* slash = strrchr(full, '/');
                    const char* name_start = slash ? slash + 1 : full;

                    size_t name_len = strlen(name_start);
                    const char* dot = strrchr(name_start, '.');
                    if(dot) name_len = dot - name_start;
                    if(name_len >= sizeof(app->save_filename))
                        name_len = sizeof(app->save_filename) - 1;

                    memcpy(app->save_filename, name_start, name_len);
                    app->save_filename[name_len] = '\0';
                } else {
                    snprintf(app->save_filename, sizeof(app->save_filename), "capture");
                }
                furi_string_free(auto_path);

                if(app->save_protocol) furi_string_free(app->save_protocol);
                app->save_protocol = protocol;
                app->save_history_idx = app->txrx->idx_menu_chosen;
                app->save_from_saved_info = false;

                text_input_reset(app->text_input);
                text_input_set_header_text(app->text_input, "Save filename:");
                text_input_set_result_callback(
                    app->text_input,
                    protopirate_scene_receiver_info_text_input_callback,
                    app,
                    app->save_filename,
                    sizeof(app->save_filename),
                    false);

                view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewTextInput);
            }
            consumed = true;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoSaveConfirm) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->save_history_idx);
            if(ff) {
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

            if(app->save_protocol) {
                furi_string_free(app->save_protocol);
                app->save_protocol = NULL;
            }

            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            consumed = true;
        }

#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == ProtoPirateCustomEventReceiverInfoEmulate &&
           app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
            FuriString* hist_path = furi_string_alloc();
            if(protopirate_history_get_capture_path(
                   app->txrx->history, app->txrx->idx_menu_chosen, hist_path)) {
                protopirate_history_release_scratch(app->txrx->history);
                if(app->loaded_file_path) furi_string_free(app->loaded_file_path);
                app->loaded_file_path = furi_string_alloc_set(hist_path);
                furi_string_free(hist_path);
                FURI_LOG_I(
                    TAG,
                    "Emulate from history file: %s",
                    furi_string_get_cstr(app->loaded_file_path));
                scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
            } else {
                furi_string_free(hist_path);
                FURI_LOG_E(TAG, "No capture path for index %d", app->txrx->idx_menu_chosen);
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        }
#endif
    }

    return consumed;
}

void protopirate_scene_receiver_info_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    widget_reset(app->widget);
    if(app->txrx && app->txrx->history) {
        protopirate_history_release_scratch(app->txrx->history);
    }
    protopirate_psa_bf_plugin_unload_if_idle(app);
}
