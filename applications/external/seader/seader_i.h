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
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/helpers/nfc_data_generator.h>

// ASN1
#include <asn_system.h>
#include <asn_internal.h>
#include <asn_codecs_prim.h>
#include <asn_codecs.h>
#include <asn_application.h>

#include <Payload.h>
#include <FrameProtocol.h>

#include "wiegand_interface_fal/interface.h"
#include "hf_interface_fal/hf_interface.h"
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <power/power_service/power.h>
#include <expansion/expansion.h>

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
#include "board_power_lifecycle.h"
#include "sam_startup_ui.h"
#include "sam_key_label.h"
#include "uhf_snmp_probe.h"
#include "uhf_status_label.h"

#define WORKER_ALL_RX_EVENTS                                                      \
    (WorkerEvtStop | WorkerEvtRxDone | WorkerEvtCfgChange | WorkerEvtLineCfgSet | \
     WorkerEvtCtrlLineSet | WorkerEvtSamTxComplete)
#define WORKER_ALL_TX_EVENTS (WorkerEvtTxStop | WorkerEvtSamRx)

#define SEADER_TEXT_STORE_SIZE         96
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
    SeaderCustomEventBoardAutoRecover,
    SeaderCustomEventBoardPowerLost,
    SeaderCustomEventStartDetect,
    SeaderCustomEventBeginRead,
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

typedef struct {
    SeaderCredentialType detected_card_types[SEADER_MAX_DETECTED_CARD_TYPES];
    size_t detected_card_type_count;
    SeaderCredentialType selected_read_type;
} SeaderHfModeContext;

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
    bool board_power_enabled;
    bool board_power_owned;
    bool expansion_disabled;
    SeaderBoardStatus board_status;
    SeaderStartupStage startup_stage;
    uint8_t board_retry_remaining;
    bool board_power_loss_pending;
    bool board_auto_recover_pending;
    bool board_auto_recover_resume_read;
    SeaderCredentialType board_auto_recover_read_type;
    uint8_t board_power_monitor_tick_divider;
    uint32_t board_power_loss_started_at;
    bool is_debug_enabled;
    Power* power;
    Expansion* expansion;
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
    uint8_t sam_version[2];
    uint8_t ATR[SEADER_MAX_ATR_SIZE];
    size_t ATR_len;
    SeaderSamKeyProbeStatus sam_key_probe_status;
    SeaderUhfProbeStatus uhf_probe_status;
    char sam_key_label[SEADER_SAM_KEY_LABEL_MAX_LEN];
    char uhf_status_label[SEADER_UHF_STATUS_LABEL_MAX_LEN];
    SeaderUhfSnmpProbe snmp_probe;
    SeaderHfModeContext hf_mode_ctx;
    bool hf_mode_active;

    char save_name_buf[SEADER_CRED_NAME_MAX_LEN + 1];
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

    PluginManager* plugin_manager;
    PluginWiegand* plugin_wiegand;
    PluginManager* hf_plugin_manager;
    PluginHf* plugin_hf;
    void* hf_plugin_ctx;
    SeaderModeRuntime mode_runtime;
    SeaderHfSessionState hf_session_state;
    SeaderHfTeardownAction hf_teardown_action;
    SeaderHfReadState hf_read_state;
    SeaderHfReadFailureReason hf_read_failure_reason;
    uint32_t hf_read_last_progress_tick;
    bool loading_popup_enabled;
    bool start_scene_active;
    bool sam_present_menu_guard_active;

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

void seader_blink_start(Seader* seader);

void seader_blink_stop(Seader* seader);

void seader_nfc_loading_callback(void* context, bool show);

TextInput* seader_get_text_input(Seader* seader);

TextBox* seader_get_text_box(Seader* seader);

Widget* seader_get_widget(Seader* seader);

void seader_show_loading_popup(void* context, bool show);

bool seader_hf_mode_activate(Seader* seader);
void seader_hf_mode_deactivate(Seader* seader);
SeaderCredentialType seader_hf_mode_get_selected_read_type(const Seader* seader);
void seader_hf_mode_set_selected_read_type(Seader* seader, SeaderCredentialType type);
void seader_hf_mode_set_detected_types(
    Seader* seader,
    const SeaderCredentialType* types,
    size_t count);
size_t seader_hf_mode_get_detected_type_count(const Seader* seader);
const SeaderCredentialType* seader_hf_mode_get_detected_types(const Seader* seader);
void seader_hf_mode_clear_detected_types(Seader* seader);
