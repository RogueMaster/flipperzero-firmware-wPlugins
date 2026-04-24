// scenes/protopirate_scene_receiver_info.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "proto_pirate_icons.h"

#define TAG "ProtoPirateReceiverInfo"

static bool is_emu_off = false;

static void protopirate_scene_receiver_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context);

static void protopirate_scene_receiver_info_text_input_callback(void* context) {
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, ProtoPirateCustomEventReceiverInfoSaveConfirm);
}

static void psa_bf_done_cb_receiver_info(void* context) {
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, ProtoPirateCustomEventPsaBruteforceComplete);
}

static bool psa_item_needs_bruteforce(ProtoPirateApp* app) {
    FlipperFormat* ff =
        protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(!ff) return false;
    FuriString* s = furi_string_alloc();
    flipper_format_rewind(ff);
    if(!flipper_format_read_string(ff, "Protocol", s) || furi_string_cmp_str(s, "PSA") != 0) {
        furi_string_free(s);
        return false;
    }
    flipper_format_rewind(ff);
    bool has_key = flipper_format_read_string(ff, "Key", s);
    if(!has_key) {
        furi_string_free(s);
        return false;
    }
    flipper_format_rewind(ff);
    bool has_serial = flipper_format_read_string(ff, "Serial", s);
    furi_string_free(s);
    return !has_serial;
}

#define PSA_BF_PROGRESS_BAR_X 62
#define PSA_BF_PROGRESS_BAR_W 64
#define PSA_BF_PROGRESS_BAR_Y 24
#define PSA_BF_PROGRESS_BAR_H 8

static void protopirate_receiver_info_show_bf_progress(ProtoPirateApp* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 0, 5, &I_DolphinWait_59x54);
    widget_add_string_element(
        app->widget, 62, 0, AlignLeft, AlignTop, FontPrimary, "Bruteforcing...");
    PsaBfState* s = app->psa_bf_state;
    uint32_t cur = s->progress_current;
    uint32_t total = s->progress_total;
    uint32_t pct_tenths = total ? (uint32_t)((uint64_t)cur * 1000 / total) : 0;
    if(pct_tenths > 1000) pct_tenths = 1000;

    FuriString* pct_str =
        furi_string_alloc_printf("%lu.%u%%", pct_tenths / 10, (unsigned)(pct_tenths % 10));
    widget_add_string_element(
        app->widget, 62, 12, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(pct_str));
    furi_string_free(pct_str);

    widget_add_rect_element(
        app->widget,
        PSA_BF_PROGRESS_BAR_X,
        PSA_BF_PROGRESS_BAR_Y,
        PSA_BF_PROGRESS_BAR_W,
        PSA_BF_PROGRESS_BAR_H,
        2,
        false);
    static uint16_t bf_ri_frame = 0;
    bf_ri_frame++;
    uint8_t inner_w = PSA_BF_PROGRESS_BAR_W - 4;
    uint8_t block_w = 16;
    uint8_t travel = inner_w - block_w;
    uint16_t phase = (bf_ri_frame * 2) % (uint16_t)(2 * travel);
    uint8_t block_x = (phase <= travel) ? (uint8_t)phase : (uint8_t)(2 * travel - phase);
    widget_add_rect_element(
        app->widget,
        PSA_BF_PROGRESS_BAR_X + 2 + block_x,
        PSA_BF_PROGRESS_BAR_Y + 2,
        block_w,
        PSA_BF_PROGRESS_BAR_H - 4,
        0,
        true);
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
        if(flipper_format_read_string(ff, "Protocol", protocol)) {
            if(furi_string_cmp_str(protocol, "PSA") == 0) is_psa = true;
            if(furi_string_cmp_str(protocol, "Scher-Khan") == 0)
                is_emu_off = true;
            else if(furi_string_cmp_str(protocol, "Kia V5") == 0)
                is_emu_off = true;
            else
                is_emu_off = false;
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

    bool psa_needs_bf = is_psa && psa_item_needs_bruteforce(app);
    if(psa_needs_bf) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeLeft,
            "Brute force",
            protopirate_scene_receiver_info_widget_callback,
            app);
    }
#ifdef ENABLE_EMULATE_FEATURE
    else if(!is_emu_off) {
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

static void
    protopirate_receiver_info_show_bf_result(ProtoPirateApp* app, uint8_t status, PsaBfState* s) {
    UNUSED(s);
    widget_reset(app->widget);
    const char* title = (status == PSA_BF_STATUS_FOUND)     ? "Found!" :
                        (status == PSA_BF_STATUS_CANCELLED) ? "Cancelled" :
                                                              "Not found";
    if(status == PSA_BF_STATUS_FOUND) {
        widget_add_icon_element(app->widget, 0, 3, &I_DolphinDone_80x58);
        widget_add_string_element(app->widget, 82, 32, AlignLeft, AlignCenter, FontPrimary, title);
        widget_add_button_element(
            app->widget,
            GuiButtonTypeCenter,
            "OK",
            protopirate_scene_receiver_info_widget_callback,
            app);
    } else if(status == PSA_BF_STATUS_CANCELLED) {
        widget_add_string_element(app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
        widget_add_icon_element(app->widget, (128 - 45) / 2, 14, &I_WarningDolphin_45x42);
    } else {
        widget_add_string_element(app->widget, 64, 0, AlignCenter, AlignTop, FontPrimary, title);
    }
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
            if(!app->psa_bf_thread && psa_item_needs_bruteforce(app)) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventReceiverInfoBruteforceStart);
            }
#ifdef ENABLE_EMULATE_FEATURE
            else if(!is_emu_off) {
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

    is_emu_off = false;

    if(app->psa_bf_thread && app->psa_bf_state) {
        if(app->psa_bf_state->status == PSA_BF_STATUS_RUNNING) {
            protopirate_receiver_info_show_bf_progress(app);
        } else {
            protopirate_receiver_info_show_bf_result(
                app, app->psa_bf_state->status, app->psa_bf_state);
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
        return;
    }

    protopirate_receiver_info_build_normal_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

static void psa_bf_finish_and_show_result(ProtoPirateApp* app) {
    PsaBfState* s = app->psa_bf_state;
    uint8_t status = s->status;
    if(app->psa_bf_thread) {
        furi_thread_join(app->psa_bf_thread);
        furi_thread_free(app->psa_bf_thread);
        app->psa_bf_thread = NULL;
    }
    if(status == PSA_BF_STATUS_FOUND) {
        FlipperFormat* ff =
            protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
        if(ff) {
            flipper_format_rewind(ff);
            flipper_format_insert_or_update_uint32(ff, "Serial", &s->decrypted_serial, 1);
            uint32_t btn = s->decrypted_button;
            flipper_format_insert_or_update_uint32(ff, "Btn", &btn, 1);
            flipper_format_insert_or_update_uint32(ff, "Cnt", &s->decrypted_counter, 1);
            uint32_t type = s->decrypted_type;
            flipper_format_insert_or_update_uint32(ff, "Type", &type, 1);
            uint32_t crc_val = s->decrypted_crc;
            flipper_format_insert_or_update_uint32(ff, "CRC", &crc_val, 1);
            flipper_format_insert_or_update_uint32(ff, "Seed", &s->decrypted_seed, 1);
        }
        FuriString* new_str = furi_string_alloc_printf(
            "PSA 128bit\r\n"
            "Key1:%08lX%08lX\r\n"
            "Key2:%04X\r\n"
            "Btn:%02X\r\n"
            "Ser:%06lX\r\n"
            "Cnt:%lX\r\n"
            "Type:%02X\r\n"
            "Sd:%06lX",
            (unsigned long)s->key1_high,
            (unsigned long)s->key1_low,
            (unsigned int)(s->key2_low & 0xFFFF),
            (unsigned int)s->decrypted_button,
            (unsigned long)s->decrypted_serial,
            (unsigned long)s->decrypted_counter,
            (unsigned int)s->decrypted_type,
            (unsigned long)s->decrypted_seed);
        protopirate_history_set_item_str(
            app->txrx->history, app->txrx->idx_menu_chosen, furi_string_get_cstr(new_str));
        furi_string_free(new_str);
        protopirate_history_commit_loaded(app->txrx->history);
    }
    if(status == PSA_BF_STATUS_FOUND) {
        protopirate_receiver_info_show_bf_result(app, status, s);

    } else {
        free(app->psa_bf_state);
        app->psa_bf_state = NULL;
        protopirate_receiver_info_show_bf_result(app, status, NULL);
    }
}

bool protopirate_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->psa_bf_state && app->psa_bf_state->status == PSA_BF_STATUS_FOUND) {
            protopirate_receiver_info_build_normal_widget(app);
            free(app->psa_bf_state);
            app->psa_bf_state = NULL;
            consumed = true;
        } else if(
            app->psa_bf_thread && app->psa_bf_state &&
            app->psa_bf_state->status == PSA_BF_STATUS_RUNNING) {
            app->psa_bf_state->cancel = 1;
            consumed = true;
        }
        return consumed;
    }

    if(event.type == SceneManagerEventTypeTick) {
        if(app->psa_bf_thread && app->psa_bf_state) {
            uint8_t bfst = app->psa_bf_state->status;
            if(bfst == PSA_BF_STATUS_IDLE || bfst == PSA_BF_STATUS_RUNNING) {
                protopirate_receiver_info_show_bf_progress(app);
                consumed = true;
            } else {
                psa_bf_finish_and_show_result(app);
                consumed = true;
            }
        }
        return consumed;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventPsaBruteforceComplete) {
            if(app->psa_bf_state) {
                psa_bf_finish_and_show_result(app);
            }
            consumed = true;
            return consumed;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoBruteforceStart) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
            if(!ff || !psa_item_needs_bruteforce(app)) {
                consumed = true;
                return consumed;
            }
            PsaBfState* state = malloc(sizeof(PsaBfState));
            if(!state) {
                notification_message(app->notifications, &sequence_error);
                consumed = true;
                return consumed;
            }
            if(!psa_bf_state_from_flipper_format(state, ff)) {
                free(state);
                notification_message(app->notifications, &sequence_error);
                consumed = true;
                return consumed;
            }
            state->on_done = psa_bf_done_cb_receiver_info;
            state->on_done_ctx = app;
            app->psa_bf_state = state;
            app->psa_bf_thread =
                furi_thread_alloc_ex("PsaBf", 2048, psa_brute_force_thread_entry, state);
            if(!app->psa_bf_thread) {
                free(state);
                app->psa_bf_state = NULL;
                notification_message(app->notifications, &sequence_error);
                consumed = true;
                return consumed;
            }
            furi_thread_start(app->psa_bf_thread);
            protopirate_receiver_info_show_bf_progress(app);
            consumed = true;
            return consumed;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoBruteforceCancel) {
            if(app->psa_bf_state && app->psa_bf_state->status == PSA_BF_STATUS_FOUND) {
                protopirate_receiver_info_build_normal_widget(app);
                free(app->psa_bf_state);
                app->psa_bf_state = NULL;
            } else if(app->psa_bf_state && app->psa_bf_state->status == PSA_BF_STATUS_RUNNING) {
                app->psa_bf_state->cancel = 1;
            } else {
                if(app->psa_bf_state) {
                    psa_bf_finish_and_show_result(app);
                }
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
            return consumed;
        }

        if(event.event == ProtoPirateCustomEventReceiverInfoSave) {
            FlipperFormat* ff =
                protopirate_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
            if(ff) {
                // Read protocol name for default filename
                FuriString* protocol = furi_string_alloc();
                flipper_format_rewind(ff);
                if(!flipper_format_read_string(ff, "Protocol", protocol)) {
                    furi_string_set_str(protocol, "Unknown");
                }

                // Clean protocol name for filename
                furi_string_replace_all(protocol, "/", "_");
                furi_string_replace_all(protocol, " ", "_");

                // Get the next auto-generated filename (just the name part)
                FuriString* auto_path = furi_string_alloc();
                if(protopirate_storage_get_next_filename(
                       furi_string_get_cstr(protocol), auto_path)) {
                    // Extract just the filename without folder and extension
                    const char* full = furi_string_get_cstr(auto_path);
                    const char* slash = strrchr(full, '/');
                    const char* name_start = slash ? slash + 1 : full;

                    // Copy without extension
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

                // Store context for when text input confirms
                if(app->save_protocol) furi_string_free(app->save_protocol);
                app->save_protocol = protocol; // transfer ownership
                app->save_history_idx = app->txrx->idx_menu_chosen;
                app->save_from_saved_info = false;

                // Configure and show text input
                text_input_reset(app->text_input);
                text_input_set_header_text(app->text_input, "Save filename:");
                text_input_set_result_callback(
                    app->text_input,
                    protopirate_scene_receiver_info_text_input_callback,
                    app,
                    app->save_filename,
                    sizeof(app->save_filename),
                    false); // don't clear default text

                view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewTextInput);
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

            // Clean up save protocol string
            if(app->save_protocol) {
                furi_string_free(app->save_protocol);
                app->save_protocol = NULL;
            }

            // Return to the receiver info widget
            view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
            consumed = true;
        }

#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == ProtoPirateCustomEventReceiverInfoEmulate && !is_emu_off) {
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
}
