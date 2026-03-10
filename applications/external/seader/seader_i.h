#pragma once
#define ASN_EMIT_DEBUG 0

#include <stdlib.h> // malloc
#include <stdint.h> // uint32_t
#include <stdarg.h> // __VA_ARGS__
#include <string.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <seader_icons.h>

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>

#include <input/input.h>

#include <lib/nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/helpers/nfc_data_generator.h>

// ASN1
#include <asn_system.h>
#include <asn_internal.h>
#include <asn_codecs_prim.h>
#include <asn_codecs.h>
#include <asn_application.h>

#include <Payload.h>
#include <FrameProtocol.h>

#include "plugin/interface.h"
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>

#include "protocol/picopass_poller.h"
#include "scenes/seader_scene.h"

#include "seader_bridge.h"
#include "seader.h"
#include "ccid.h"
#include "uart.h"
#include "lrc.h"
#include "t_1.h"
#include "seader_worker.h"
#include "seader_credential.h"
#include "apdu_log.h"
#include "sam_key_label.h"
#include "uhf_snmp_probe.h"
#include "uhf_status_label.h"

#define WORKER_ALL_RX_EVENTS                                                      \
    (WorkerEvtStop | WorkerEvtRxDone | WorkerEvtCfgChange | WorkerEvtLineCfgSet | \
     WorkerEvtCtrlLineSet | WorkerEvtSamTxComplete)
#define WORKER_ALL_TX_EVENTS (WorkerEvtTxStop | WorkerEvtSamRx)

#define SEADER_TEXT_STORE_SIZE         128
#define SEADER_MAX_ATR_SIZE            33
#define MAX_FRAME_HEADERS              32
#define SEADER_MAX_DETECTED_CARD_TYPES 3

enum SeaderCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    SeaderCustomEventReserved = 100,

    SeaderCustomEventViewExit,
    SeaderCustomEventWorkerExit,
    SeaderCustomEventByteInputDone,
    SeaderCustomEventTextInputDone,

    SeaderCustomEventPollerDetect,
    SeaderCustomEventPollerSuccess,
    SeaderCustomEventSamStatusUpdated,
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),

    WorkerEvtTxStop = (1 << 2),
    WorkerEvtSamRx = (1 << 3),
    WorkerEvtSamTxComplete = (1 << 4),

    WorkerEvtCfgChange = (1 << 5),

    WorkerEvtLineCfgSet = (1 << 6),
    WorkerEvtCtrlLineSet = (1 << 7),
} WorkerEvtFlags;

typedef struct {
    uint16_t total_lines;
    uint16_t current_line;
} SeaderAPDURunnerContext;

typedef enum {
    SeaderSamStateIdle,
    SeaderSamStateDetectPending,
    SeaderSamStateConversation,
    SeaderSamStateFinishing,
    SeaderSamStateClearPending,
    SeaderSamStateVersionPending,
    SeaderSamStateSerialPending,
    SeaderSamStateCapabilityPending,
} SeaderSamState;

typedef enum {
    SeaderSamIntentNone,
    SeaderSamIntentReadPacs2,
    SeaderSamIntentConfig,
    SeaderSamIntentMaintenance,
} SeaderSamIntent;

struct Seader {
    bool revert_power;
    bool is_debug_enabled;
    SeaderWorker* worker;
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notifications;
    SceneManager* scene_manager;
    SeaderUartBridge* uart;
    SeaderCredential* credential;
    SamCommand_PR samCommand;
    SeaderSamState sam_state;
    SeaderSamIntent sam_intent;
    bool sam_present;
    uint8_t ATR[SEADER_MAX_ATR_SIZE];
    size_t ATR_len;
    char sam_key_label[SEADER_SAM_KEY_LABEL_MAX_LEN];
    char uhf_status_label[SEADER_UHF_STATUS_LABEL_MAX_LEN];
    SeaderUhfSnmpProbe snmp_probe;

    char text_store[SEADER_TEXT_STORE_SIZE + 1];
    char read_error[SEADER_TEXT_STORE_SIZE + 1];
    FuriString* text_box_store;

    // Reusable strings to optimize scene string allocations
    FuriString* temp_string1;
    FuriString* temp_string2;
    FuriString* temp_string3;
    FuriString* temp_string4;

    // Common Views
    Submenu* submenu;
    Popup* popup;
    Loading* loading;
    TextInput* text_input;
    TextBox* text_box;
    Widget* widget;

    Nfc* nfc;
    NfcPoller* poller;
    PicopassPoller* picopass_poller;

    NfcDevice* nfc_device;
    SeaderCredentialType detected_card_types[SEADER_MAX_DETECTED_CARD_TYPES];
    size_t detected_card_type_count;
    SeaderCredentialType selected_read_type;

    PluginManager* plugin_manager;
    PluginWiegand* plugin_wiegand;

    APDULog* apdu_log;
    SeaderAPDURunnerContext apdu_runner_ctx;
};

struct SeaderPollerContainer {
    Iso14443_4aPoller* iso14443_4a_poller;
    MfClassicPoller* mfc_poller;
    PicopassPoller* picopass_poller;
};

typedef enum {
    SeaderViewMenu,
    SeaderViewPopup,
    SeaderViewLoading,
    SeaderViewTextInput,
    SeaderViewTextBox,
    SeaderViewWidget,
    SeaderViewUart,
} SeaderView;

void seader_text_store_set(Seader* seader, const char* text, ...);

void seader_text_store_clear(Seader* seader);

void seader_blink_start(Seader* seader);

void seader_blink_stop(Seader* seader);

void seader_show_loading_popup(void* context, bool show);
