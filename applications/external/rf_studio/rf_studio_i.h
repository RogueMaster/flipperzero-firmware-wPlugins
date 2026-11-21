#pragma once

#include "rfs_types.h"
#include "storage/rfs_storage.h"
#include "diagnostics/rfs_diag.h"
#include "protocol/rfs_protocol.h"
#include "radio/rfs_radio.h"
#include "radio/rfs_tx.h"
#include "signal/rfs_signal.h"
#include "remote/rfs_remote.h"
#include "capture/rfs_capture.h"
#include "ui/views/rfs_remote_view.h"
#include "ui/views/rfs_capture_view.h"
#include "ui/scenes/rfs_scene.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

typedef enum {
    RfsViewSubmenu,
    RfsViewTextInput,
    RfsViewVarList,
    RfsViewWidget,
    RfsViewDialog,
    RfsViewPopup,
    RfsViewTextBox,
    RfsViewRemote,
    RfsViewCapture,
} RfsViewId;

/* Custom events. Submenu selections use RfsEventMenuBase + item index. */
typedef enum {
    RfsEventTxDone = 1,
    RfsEventCaptureFound,
    RfsEventCaptureDecoded,
    RfsEventCaptureRaw,
    RfsEventCaptureError,
    RfsEventRemoteSend,
    RfsEventRemoteMenu,
    RfsEventCaptureOptions,
    RfsEventCaptureStop,
    RfsEventCaptureRestart,
    RfsEventTextDone,
    RfsEventDialogLeft,
    RfsEventDialogRight,
    RfsEventPopupDone,
    RfsEventResultRetry,
    RfsEventResultSave,
    RfsEventMenuBase = 1000,
} RfsEvent;

/* What a text entry is for. */
typedef enum {
    RfsTextRemoteNew,
    RfsTextRemoteRename,
    RfsTextButtonNew, /* name a freshly captured button */
    RfsTextButtonRename,
    RfsTextSignalNew, /* name a signal captured into the library */
    RfsTextSignalRename,
    RfsTextSearch,
} RfsTextPurpose;

/* What a confirmation dialog is for. */
typedef enum {
    RfsConfirmDeleteRemote,
    RfsConfirmDeleteButton,
    RfsConfirmDeleteSignal,
} RfsConfirmPurpose;

/* Why the capture screen was opened. */
typedef enum {
    RfsCaptureForNewButton,
    RfsCaptureForRerecord,
    RfsCaptureForLibrary,
} RfsCaptureTarget;

/* Why the signal list was opened. */
typedef enum {
    RfsSignalListBrowse,
    RfsSignalListPickForButton, /* replace the selected button's signal */
    RfsSignalListPickNewButton, /* create a new button from an existing signal */
} RfsSignalListMode;

/* Which file the import scene asks for. */
typedef enum {
    RfsImportSignal,
    RfsImportRemote,
} RfsImportKind;

typedef enum {
    RfsInfoSignal,
    RfsInfoDiagnostics,
} RfsInfoKind;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Storage* storage;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* var_list;
    Widget* widget;
    DialogEx* dialog;
    Popup* popup;
    TextBox* text_box;
    RfsRemoteView* remote_view;
    RfsCaptureView* capture_view;

    RfsSettings settings;
    RfsDiag* diag;
    RfsRadio* radio;
    RfsTx* tx;
    RfsCapture* capture;
    RfsSignalIndex signals;
    RfsRemote remote; /* the open remote */
    RfsRemoteList remote_list;
    bool sd_ready;

    /* Navigation state shared between scenes */
    bool favorites_only;
    char search[RFS_NAME_LEN + 1];
    int32_t button_index; /* selected button in the open remote */
    char signal_id[RFS_ID_LEN]; /* selected signal in the library */
    RfsTextPurpose text_purpose;
    char text_buffer[RFS_NAME_LEN + 1];
    RfsConfirmPurpose confirm_purpose;
    RfsCaptureTarget capture_target;
    RfsSignalListMode signal_list_mode;
    RfsImportKind import_kind;
    RfsInfoKind info_kind;

    /* Capture settings chosen for the current capture session */
    uint32_t capture_frequency; /* 0 = detect automatically */
    char capture_preset[RFS_PRESET_LEN];
    RfsRadioMode capture_radio;

    /* Message popup */
    FuriString* message_title;
    FuriString* message_text;
    uint32_t message_return; /* scene to return to, or RfsSceneNum for previous */

    FuriString* text_store; /* text box contents */
} RfStudioApp;

/* Helpers shared by scenes (rf_studio.c). */

/** Show a popup message, then return to return_scene (RfsSceneNum = previous scene). */
void rfs_app_message(RfStudioApp* app, const char* title, const char* text, uint32_t return_scene);

/** Save the open remote, reporting failure. */
bool rfs_app_save_remote(RfStudioApp* app);

/** Save the signal index, reporting failure. */
bool rfs_app_save_signals(RfStudioApp* app);

/** Radio mode for the open remote: its preference, else the global setting. */
RfsRadioMode rfs_app_remote_radio_mode(RfStudioApp* app);

/** Start transmitting a library signal. Returns false with a reason in out if it cannot start. */
bool rfs_app_send_signal(
    RfStudioApp* app,
    const char* signal_id,
    RfsRadioMode mode,
    FuriString* out);

/** Leave the capture flow back to where it was started from. */
void rfs_app_exit_capture(RfStudioApp* app);

/** Open the capture screen for a new or re-recorded button, suggesting the remote's frequency. */
void rfs_app_start_button_capture(RfStudioApp* app, RfsCaptureTarget target);

/** Open the capture screen to add a signal straight to the library. */
void rfs_app_start_library_capture(RfStudioApp* app);

/** Save the capture result into the library. Writes the new signal ID to out_id. */
bool rfs_app_store_capture(RfStudioApp* app, const char* name, char* out_id);

/** Save the capture result and attach it to a new button or the button being re-recorded,
 *  then return to the remote. */
void rfs_app_finish_button_capture(RfStudioApp* app, const char* button_name);

/** Remember a frequency on the open remote for its next button (AC-DYN-013). */
void rfs_app_remember_frequency(RfStudioApp* app, uint32_t frequency, const char* preset);

/** Format a frequency as "433.92". */
void rfs_format_mhz(uint32_t frequency, char* out, size_t size);

/** Submenu callback that forwards RfsEventMenuBase + index. */
void rfs_app_submenu_callback(void* context, uint32_t index);
