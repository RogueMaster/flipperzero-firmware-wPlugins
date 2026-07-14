#include "../saflip.h"
#include "../util/saflok/formats/mfc.h"
#include "protocols/mf_classic/mf_classic.h"
#include "protocols/mf_classic/mf_classic_listener.h"

enum {
    SaflipSceneEmulateEventMFCEvent,
};

NfcCommand callback(NfcGenericEvent event, void* context) {
    SaflipApp* app = context;

    switch(event.protocol) {
    case NfcProtocolMfClassic:
        MfClassicListenerEvent* evt = event.event_data;
        if(evt->type == MfClassicListenerEventTypeAuthContextFullCollected)
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SaflipSceneEmulateEventMFCEvent);

        break;
    default:
        FURI_LOG_W(TAG, "Unknown protocol: %d", event.protocol);
    }

    return NfcCommandContinue;
}

void saflip_scene_emulate_setup_popup(void* context) {
    SaflipApp* app = context;

    // Reset popup
    popup_set_header(app->popup, "Emulating", 67, 13, AlignLeft, AlignTop);
    popup_set_icon(app->popup, 0, 3, &I_NFC_dolphin_emulation_51x64);
    popup_set_text(app->popup, app->text_store, 90, 28, AlignCenter, AlignTop);
    popup_set_callback(app->popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
}

void saflip_scene_emulate_on_enter(void* context) {
    SaflipApp* app = context;

    if(!saflok_generate(app->nfc_device, app->uid, app->uid_len, app->data)) {
        FURI_LOG_E(TAG, "Unsupported format for emulation!");
        return;
    }

    if(app->data->format == SaflipFormatMifareClassic) {
        saflok_generate_mf_classic_variable_keys(
            app->nfc_device, app->keys, app->variable_keys, app->variable_keys_optional_function);
        saflok_generate_mf_classic_logs(app->nfc_device, app->log, app->log_entries);
    }

    NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);

    const NfcDeviceData* data = nfc_device_get_data(app->nfc_device, protocol);
    app->nfc_listener = nfc_listener_alloc(app->nfc, protocol, data);
    nfc_listener_start(app->nfc_listener, callback, app);

    const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &app->uid_len);
    memcpy(app->uid, uid, app->uid_len);

    FuriString* desc = furi_string_alloc();
    furi_string_printf(desc, "%s", nfc_device_get_name(app->nfc_device, NfcDeviceNameTypeFull));
    for(uint8_t i = 0; i < app->uid_len; i++) {
        if(i > 0) furi_string_cat_printf(desc, ":");
        if(i % 4 == 0) furi_string_cat_printf(desc, "\n");
        furi_string_cat_printf(desc, "%02X", uid[i]);
    }

    // Save description to text store and show popup
    sprintf(app->text_store, "%s", furi_string_get_cstr(desc));
    furi_string_free(desc);
    saflip_scene_emulate_setup_popup(app);
    app->num_store = app->log_entries;

    notification_message(app->notifications, &sequence_blink_start_magenta);
}

void saflok_scene_emulate_check_log_data(SaflipApp* app) {
    if(app->data->format == SaflipFormatMifareClassic) {
        app->log_entries = saflok_parse_mf_classic_logs(app->nfc_device, app->log);

        for(size_t idx = app->num_store; idx < app->log_entries; idx++) {
            LogEntry entry = app->log[idx];

            char* text = saflok_log_entry_description(entry);
            if(text == NULL) {
                FuriString* msg = furi_string_alloc_printf(
                    "TimeSet=%d, IsDST=%d\nNew=%d, LetOpen=%d\nDeadbolt=%d, LockProb=%d\nLatched=%d, LowBatt=%d\nLockID=%d, DiagCode=%d\n\n%04d-%02d-%02d %02d:%02d",
                    entry.time_is_set,
                    entry.is_dst,
                    entry.new_key,
                    entry.let_open,
                    entry.deadbolt,
                    entry.lock_problem,
                    entry.lock_latched,
                    entry.low_battery,
                    entry.lock_id,
                    entry.diagnostic_code,
                    entry.time.year,
                    entry.time.month,
                    entry.time.day,
                    entry.time.hour,
                    entry.time.minute);
                text = strdup(furi_string_get_cstr(msg));
                furi_string_free(msg);
            }

            FuriString* title_str =
                furi_string_alloc_printf("New log entry (%d total)", app->log_entries);
            char* title = strdup(furi_string_get_cstr(title_str));
            furi_string_free(title_str);

            popup_reset(app->popup);
            popup_set_header(app->popup, title, 0, 0, AlignLeft, AlignTop);
            popup_set_text(app->popup, text, 0, 12, AlignLeft, AlignTop);
            popup_set_timeout(app->popup, 3000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, saflip_scene_emulate_setup_popup);
            popup_set_context(app->popup, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
        }
    }
}

bool saflip_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    // Copy any data written by lock to actual NFC data storage
    nfc_device_set_data(
        app->nfc_device,
        NfcProtocolMfClassic,
        nfc_listener_get_data(app->nfc_listener, NfcProtocolMfClassic));

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SaflipSceneEmulateEventMFCEvent:
            saflok_scene_emulate_check_log_data(app);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void saflip_scene_emulate_on_exit(void* context) {
    SaflipApp* app = context;

    nfc_listener_stop(app->nfc_listener);
    nfc_listener_free(app->nfc_listener);

    // Clear view
    popup_reset(app->popup);
    notification_message(app->notifications, &sequence_blink_stop);
}
