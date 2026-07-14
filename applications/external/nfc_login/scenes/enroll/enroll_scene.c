#include "enroll_scene.h"
#include "scenes/cards/cards_scene.h"
#include "scenes/settings/settings_scene.h"
#include "../../storage/nfc_login_card_storage.h"

#include <ctype.h>

NfcCommand iso14443_3a_async_callback(NfcGenericEvent event, void* context) {
    AsyncPollerContext* ctx = (AsyncPollerContext*)context;

    if(event.protocol == NfcProtocolIso14443_3a) {
        Iso14443_3aPollerEvent* poller_event = (Iso14443_3aPollerEvent*)event.event_data;

        if(poller_event->type == Iso14443_3aPollerEventTypeReady) {
            const NfcDeviceData* device_data = nfc_poller_get_data(ctx->poller);
            if(device_data) {
                const Iso14443_3aData* poller_data = (const Iso14443_3aData*)device_data;
                iso14443_3a_copy(&ctx->iso14443_3a_data, poller_data);
                ctx->error = Iso14443_3aErrorNone;
                ctx->detected = true;
                furi_thread_flags_set(ctx->thread_id, ISO14443_3A_ASYNC_FLAG_COMPLETE);
                return NfcCommandStop;
            }
        } else if(poller_event->type == Iso14443_3aPollerEventTypeError) {
            ctx->error = poller_event->data->error;
            ctx->detected = false;
            ctx->reset_counter++;
            if(ctx->reset_counter >= 3) {
                ctx->reset_counter = 0;
                furi_thread_flags_set(ctx->thread_id, ISO14443_3A_ASYNC_FLAG_COMPLETE);
                return NfcCommandReset;
            }
            furi_thread_flags_set(ctx->thread_id, ISO14443_3A_ASYNC_FLAG_COMPLETE);
            return NfcCommandStop;
        }
    }

    return NfcCommandContinue;
}

int32_t app_enroll_scan_thread(void* context) {
    App* app = context;
    Nfc* nfc = nfc_alloc();
    if(!nfc) return 0;

    AsyncPollerContext async_ctx = {
        .thread_id = furi_thread_get_current_id(),
        .reset_counter = 0,
        .detected = false,
        .error = Iso14443_3aErrorNone,
        .poller = NULL,
    };

    while(app->enrollment_scanning) {
        async_ctx.detected = false;
        async_ctx.error = Iso14443_3aErrorNone;

        NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
        async_ctx.poller = poller;
        nfc_poller_start(poller, iso14443_3a_async_callback, &async_ctx);

        furi_thread_flags_wait(ISO14443_3A_ASYNC_FLAG_COMPLETE, FuriFlagWaitAny, FuriWaitForever);
        furi_thread_flags_clear(ISO14443_3A_ASYNC_FLAG_COMPLETE);

        nfc_poller_stop(poller);
        nfc_poller_free(poller);

        furi_delay_ms(NFC_COOLDOWN_DELAY_MS);

        if(!app->enrollment_scanning) break;

        if(async_ctx.detected && async_ctx.error == Iso14443_3aErrorNone) {
            size_t uid_len = 0;
            const uint8_t* uid = iso14443_3a_get_uid(&async_ctx.iso14443_3a_data, &uid_len);
            if(uid && uid_len > 0 && uid_len <= MAX_UID_LEN) {
                app->enrollment_card.uid_len = uid_len;
                memcpy(app->enrollment_card.uid, uid, uid_len);
                notification_message(app->notification, &sequence_success);
                app->enrollment_scanning = false;
                if(app->widget_state == 3) {
                    view_dispatcher_send_custom_event(app->view_dispatcher, EventEditUidDone);
                } else {
                    view_dispatcher_send_custom_event(app->view_dispatcher, EventPromptPassword);
                }
                break;
            }
        }

        furi_delay_ms(NFC_ENROLL_SCAN_DELAY_MS);
    }
    nfc_free(nfc);
    return 0;
}

void app_text_input_result_callback(void* context) {
    App* app = context;

    if(app->enrollment_state == EnrollmentStateName) {
        if(strlen(app->enrollment_card.name) > 0) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventStartScan);
        } else {
            app->enrollment_state = EnrollmentStateNone;
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        }
    } else if(app->enrollment_state == EnrollmentStatePassword) {
        if(strlen(app->enrollment_card.password) > 0 && app->enrollment_card.uid_len > 0) {
            if(app->card_count < MAX_CARDS) {
                memcpy(&app->cards[app->card_count], &app->enrollment_card, sizeof(NfcCard));
                app->card_count++;
                if(app_save_cards(app)) {
                    notification_message(app->notification, &sequence_success);
                } else {
                    FURI_LOG_E(TAG, "app_text_input_result_callback: Save failed, removing card");
                    app->card_count--;
                    notification_message(app->notification, &sequence_error);
                }
            } else {
                FURI_LOG_E(
                    TAG, "app_text_input_result_callback: Max cards reached (%d)", MAX_CARDS);
                notification_message(app->notification, &sequence_error);
            }
        } else {
            FURI_LOG_E(
                TAG,
                "app_text_input_result_callback: Invalid password or UID (password_len=%zu, uid_len=%zu)",
                strlen(app->enrollment_card.password),
                app->enrollment_card.uid_len);
        }
        app->enrollment_state = EnrollmentStateNone;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
    }
}

bool app_import_nfc_file(App* app, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[128];
        size_t len = 0;
        uint8_t ch = 0;
        memset(&app->enrollment_card, 0, sizeof(NfcCard));
        while(true) {
            len = 0;
            while(len < sizeof(line) - 1) {
                uint16_t rd = storage_file_read(file, &ch, 1);
                if(rd == 0) break;
                if(ch == '\n') break;
                line[len++] = (char)ch;
            }
            if(len == 0) break;
            line[len] = '\0';
            if(len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
            if(strncmp(line, "UID:", 4) == 0) {
                const char* p = line + 4;
                while(*p == ' ')
                    p++;
                size_t idx = 0;
                while(*p && idx < MAX_UID_LEN) {
                    while(*p == ' ')
                        p++;
                    if(!isxdigit((unsigned char)p[0])) break;
                    unsigned int byte_val = 0;
                    if(sscanf(p, "%2x", &byte_val) == 1) {
                        app->enrollment_card.uid[idx++] = (uint8_t)byte_val;
                        p++;
                        if(isxdigit((unsigned char)*p)) p++;
                    } else {
                        break;
                    }
                    while(*p == ' ')
                        p++;
                }
                if(idx > 0) {
                    app->enrollment_card.uid_len = idx;
                    ok = true;
                    break;
                }
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(ok) {
        const char* slash = strrchr(path, '/');
        const char* fname = slash ? slash + 1 : path;
        size_t n = 0;
        while(fname[n] && fname[n] != '.' && n < sizeof(app->enrollment_card.name) - 1) {
            app->enrollment_card.name[n] = fname[n];
            n++;
        }
        app->enrollment_card.name[n] = '\0';
    }
    return ok;
}

bool app_custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    switch(event) {
    case EventAddCardStart:
        memset(&app->enrollment_card, 0, sizeof(NfcCard));
        app->enrollment_state = EnrollmentStateName;
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Enter Card Name");
        text_input_set_result_callback(
            app->text_input,
            app_text_input_result_callback,
            app,
            app->enrollment_card.name,
            sizeof(app->enrollment_card.name),
            true);
#ifdef HAS_MOMENTUM_SUPPORT
        text_input_show_illegal_symbols(app->text_input, true);
#endif
        app_switch_to_view(app, ViewTextInput);
        return true;
    case EventStartScan: {
        widget_reset(app->widget);
        widget_add_icon_element(app->widget, 2, 6, &I_Scanning_123x52);
        widget_add_icon_element(app->widget, 124, 56, &I_ButtonRight_4x7);
        widget_add_string_element(
            app->widget, 117, 56, AlignRight, AlignTop, FontSecondary, "Manual");
        widget_add_string_element(
            app->widget, 0, 56, AlignRight, AlignTop, FontSecondary, "Back=Cancel");
        app_switch_to_view(app, ViewWidget);
        app->widget_state = 1;
        if(!app->enrollment_scanning) {
            app->enrollment_scanning = true;
            app->enroll_scan_thread = furi_thread_alloc();
            furi_thread_set_name(app->enroll_scan_thread, "EnrollScan");
            furi_thread_set_stack_size(app->enroll_scan_thread, 2 * 1024);
            furi_thread_set_context(app->enroll_scan_thread, app);
            furi_thread_set_callback(app->enroll_scan_thread, app_enroll_scan_thread);
            furi_thread_start(app->enroll_scan_thread);
        }
        return true;
    }
    case EventPromptPassword:
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
        return true;
    case EventEditUidDone:
        if(app->edit_card_index < app->card_count && app->enrollment_card.uid_len > 0 &&
           app->enrollment_card.uid_len <= MAX_UID_LEN) {
            app->cards[app->edit_card_index].uid_len = app->enrollment_card.uid_len;
            memcpy(
                app->cards[app->edit_card_index].uid,
                app->enrollment_card.uid,
                app->enrollment_card.uid_len);
            if(!app_save_cards(app)) {
                FURI_LOG_E(TAG, "Failed to save card after UID scan");
            }
        }
        app->widget_state = 3;
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Edit Card");
        {
            const char* items[] = {"Name", "Password", "UID (scan)", "Delete"};
            for(size_t i = 0; i < 4; i++) {
                char line[32];
                snprintf(
                    line, sizeof(line), "%s %s", (i == app->edit_menu_index) ? ">" : " ", items[i]);
                widget_add_string_element(
                    app->widget, 0, 12 + i * 12, AlignLeft, AlignTop, FontSecondary, line);
            }
        }
        widget_add_string_element(
            app->widget, 0, 60, AlignLeft, AlignTop, FontSecondary, "Back=List");
        app_switch_to_view(app, ViewWidget);
        return true;
    case EventManualUidEntry:
        // Stop scanning if active
        if(app->enrollment_scanning) {
            app->enrollment_scanning = false;
            if(app->enroll_scan_thread) {
                furi_thread_join(app->enroll_scan_thread);
                furi_thread_free(app->enroll_scan_thread);
                app->enroll_scan_thread = NULL;
            }
        }
        // Set up byte input for UID entry - allow up to MAX_UID_LEN bytes
        app->enrollment_card.uid_len = MAX_UID_LEN; // Allow full length, user can enter less
        memset(app->enrollment_card.uid, 0, sizeof(app->enrollment_card.uid));
        byte_input_set_header_text(app->byte_input, "Enter UID (hex)");
        byte_input_set_result_callback(
            app->byte_input,
            app_enroll_uid_byte_input_done,
            NULL,
            app,
            app->enrollment_card.uid,
            app->enrollment_card.uid_len);
        app_switch_to_view(app, ViewByteInput);
        return true;
    default:
        return false;
    }
}

void app_enroll_uid_byte_input_done(void* context) {
    App* app = context;
    // Calculate actual UID length (find first zero byte or use full length)
    size_t actual_len = 0;
    for(size_t i = 0; i < MAX_UID_LEN; i++) {
        if(app->enrollment_card.uid[i] != 0) {
            actual_len = i + 1;
        } else {
            break;
        }
    }

    if(actual_len > 0 && actual_len <= MAX_UID_LEN) {
        app->enrollment_card.uid_len = actual_len;
        // Proceed to password entry
        view_dispatcher_send_custom_event(app->view_dispatcher, EventPromptPassword);
    } else {
        notification_message(app->notification, &sequence_error);
        app->enrollment_state = EnrollmentStateNone;
        app_switch_to_view(app, ViewSubmenu);
    }
}
