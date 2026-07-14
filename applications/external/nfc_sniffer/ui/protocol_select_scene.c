
#include "ui_i.h"

#define LOG_TAG "nfc_sniffer_protocol_select_scene"

static void submenu_callback(void* context, uint32_t index) {
    FURI_LOG_T(LOG_TAG, __func__);
    FURI_LOG_D(LOG_TAG, "Selected protocol %ld", index);

    UI* ui = (UI*)context;
    ui->nfc_protocol = index;
    scene_manager_next_scene(ui->scene_manager, View_LogDisplay);
}

void protocol_select_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    submenu_reset(ui->protocol_select_scene->protocol_menu);
    submenu_set_header(ui->protocol_select_scene->protocol_menu, "Select NFC Protocol");

    for(int i = 0; i < NfcTechNum; i++) {
        if(i == NfcTechIso14443b) {
            // ISO14443-3B does not support listening mode
            continue;
        }

        submenu_add_item(
            ui->protocol_select_scene->protocol_menu, PROTOCOL_NAMES[i], i, submenu_callback, ui);
    }

    view_dispatcher_switch_to_view(ui->view_dispatcher, View_ProtocolSelectDisplay);
}

bool protocol_select_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }

    // Default event handler
    return false;
}

void protocol_select_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    submenu_reset(ui->protocol_select_scene->protocol_menu);
}

ProtocolSelectScene* protocol_select_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    ProtocolSelectScene* protocol_select_scene = malloc(sizeof(ProtocolSelectScene));
    protocol_select_scene->protocol_menu = submenu_alloc();

    return protocol_select_scene;
}

void protocol_select_scene_free(ProtocolSelectScene* protocol_select_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(protocol_select_scene);

    submenu_free(protocol_select_scene->protocol_menu);
    free(protocol_select_scene);
}
