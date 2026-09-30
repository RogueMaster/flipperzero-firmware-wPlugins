#include "../k2_rfid_app.h"
#include "k2_scene.h"

void k2_scene_emulate_on_enter(void* context) {
    K2RfidApp* app = context;
    Popup* popup = app->popup;

    /* Ensure worker polling tasks are stopped */
    k2_worker_stop(app->worker);

    /* Ensure valid spool configuration */
    if (app->config.material_id[0] == '\0') {
        k2_rfid_app_set_default_config(app);
    } else {
        k2_rfid_app_sync_config(app);
    }

    const K2Material* mat = k2_db_find_material_by_id(app->config.material_id);
    const char* mat_name = (mat && mat->name) ? mat->name : "Hyper PLA";
    const char* col_name = k2_db_find_closest_color_name(app->config.color_hex);
    if (!col_name || strlen(col_name) == 0) {
        col_name = "White";
    }
    const char* weight = k2_db_find_weight_label_by_code(app->config.weight_code);
    if (!weight || strlen(weight) == 0) {
        weight = "1 KG";
    }

    FuriString* str = furi_string_alloc();
    furi_string_printf(
        str,
        "%s\n%s - %s\nHold against CFS slot\nPress Back to Exit",
        mat_name,
        col_name,
        weight);

    popup_reset(popup);
    popup_set_header(popup, "Emulating Spool", 64, 5, AlignCenter, AlignTop);
    popup_set_text(popup, furi_string_get_cstr(str), 64, 20, AlignCenter, AlignTop);
    furi_string_free(str);

    /* Allocate and prepare MIFARE Classic tag data */
    app->emulate_data = mf_classic_alloc();
    k2_prepare_mf_classic_data(&app->config, NULL, app->emulate_data);

    /* Allocate and start NFC listener directly on app thread */
    app->listener = nfc_listener_alloc(
        k2_worker_get_nfc(app->worker),
        NfcProtocolMfClassic,
        (const NfcDeviceData*)app->emulate_data);
    if(app->listener) {
        nfc_listener_start(app->listener, NULL, NULL);
    }

    notification_message(app->notifications, &sequence_blink_start_yellow);
    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewPopup);
}

bool k2_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_emulate_on_exit(void* context) {
    K2RfidApp* app = context;

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    if(app->emulate_data) {
        mf_classic_free(app->emulate_data);
        app->emulate_data = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
