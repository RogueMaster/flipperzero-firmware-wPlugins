#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include <infrared_worker.h>

#include "protocol/wfr_protocol.h"
#include "wfr_decode.h"
#include "wfr_storage.h"
#include "scenes/wi_fir_scene.h"

/* View IDs for ViewDispatcher */
typedef enum {
    WiFirViewSubmenu,
    WiFirViewTextInput,
    WiFirViewVariableItemList,
    WiFirViewDialogEx,
    WiFirViewPopup,
    WiFirViewLoading,
} WiFirView;

/* Main application struct */
typedef struct {
    /* GUI infrastructure */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    /* View modules */
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    DialogEx* dialog_ex;
    Popup* popup;
    Loading* loading;

    /* Storage */
    Storage* storage;

    /* NFC (allocated per-scene to avoid holding the HAL) */
    Nfc* nfc;
    NfcPoller* nfc_poller;

    /* WiFi credential data (current working set) */
    char ssid[WFR_SSID_MAX_LEN + 1];
    char password[WFR_PASS_MAX_LEN + 1];
    uint8_t security_type;
    bool hidden;

    /* Text input buffer (shared across text input scenes) */
    char text_input_buf[WFR_PASS_MAX_LEN + 1];

    /* Confirm screen text buffer */
    char confirm_text[128];

    /* NFC scan result */
    WfrWifiCreds nfc_creds;

    /* Saved networks list (populated by saved scene) */
    char saved_ssids[WFR_SAVED_MAX][WFR_SSID_MAX_LEN + 1];
    char saved_files[WFR_SAVED_MAX][WFR_FILENAME_MAX];
    uint8_t saved_count;

    /* Currently selected saved file (for delete from confirm screen) */
    char selected_saved_file[WFR_FILENAME_MAX];

    /* Settings */
    bool ack_enabled;

    /* IR ACK receive state */
    InfraredWorker* ir_worker;
    FuriTimer* ack_timer;
    WfrAckDecoder ack_decoder;
    char ack_result_text[64];
} WiFirApp;

/* App entry point */
int32_t wi_fir_app(void* p);
