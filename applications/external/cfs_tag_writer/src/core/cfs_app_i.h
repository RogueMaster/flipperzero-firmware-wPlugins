#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/number_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>

#include "src/data/cfs_data.h"
#include "src/worker/cfs_nfc_worker.h"
#include "src/storage/cfs_storage.h"
#include "src/settings/cfs_settings.h"
#include "src/core/cfs_scene.h"

typedef enum {
    CfsViewSubmenu,
    CfsViewWidget,
    CfsViewPopup,
    CfsViewNumberInput,
    CfsViewByteInput,
    CfsViewTextInput,
    CfsViewVariableItemList,
} CfsViewId;

// The first 8 values mirror CfsNfcWorkerEvent order exactly, so the worker
// callback can map its event to a custom event with a simple cast.
typedef enum {
    CfsCustomEventWorkerSuccess = 0,
    CfsCustomEventWorkerBlankTag,
    CfsCustomEventWorkerNotPresent,
    CfsCustomEventWorkerWrongType,
    CfsCustomEventWorkerAuthFailed,
    CfsCustomEventWorkerReadFailed,
    CfsCustomEventWorkerWriteFailed,
    CfsCustomEventWorkerVerifyFailed,

    CfsCustomEventButtonWrite = 100,
    CfsCustomEventButtonWriteDual,
    CfsCustomEventButtonSave,
    CfsCustomEventButtonRetry,
    CfsCustomEventButtonClone,
    CfsCustomEventButtonEdit,
    CfsCustomEventButtonReread,
    CfsCustomEventButtonWriteTag2, // dual-tag: proceed to second tag
    CfsCustomEventButtonErase, // confirm erase
    CfsCustomEventButtonOverwrite, // confirm overwriting an existing saved record
    CfsCustomEventButtonDetails, // open the raw/details view from a read result

    // Start-menu item ids live in this same custom-event namespace (they're sent
    // via view_dispatcher_send_custom_event too). Based well past the worker
    // (0-7) and button (100+) ranges so a worker event queued during teardown can
    // never alias a menu action. See src/scenes/menu/cfs_scene_start.c.
    CfsCustomEventMenuBase = 200,
} CfsCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Storage* storage;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    NumberInput* number_input;
    ByteInput* byte_input;
    TextInput* text_input;
    VariableItemList* variable_item_list;

    CfsNfcWorker* worker;
    CfsSettings settings;

    // Current read result / loaded record.
    CfsTagData tag_data;
    uint8_t uid[10];
    size_t uid_len;
    bool is_blank;
    bool result_read_only; // ReadResult opened to view a saved record, not a live read
    uint8_t tag_plain[CFS_PLAINTEXT_SIZE]; // decrypted 48-byte payload from a live read
    bool has_tag_plain; // true if tag_plain holds a live read (false for saved records)

    // Write staging.
    CfsTagData pending;
    bool write_exact; // clone: write app->tag_plain verbatim instead of encoding pending
    bool dual_tag;
    uint8_t write_tag_index; // 0 = first tag, 1 = second tag
    size_t pending_weight_index; // selected weight in the write wizard
    bool has_source_serial; // clone/edit: pending.serial holds a source serial to keep in Keep mode

    // Save-name flow.
    char save_name[CFS_STORAGE_NAME_MAX];
    bool save_from_write; // true: save app->pending (no uid); false: save tag_data + uid

    // Scratch buffers for input views.
    uint8_t color_bytes[3];
    int32_t number_value;
    char text_scratch[64];

    // Saved-tags scene: full path of the .nfc chosen via the file browser.
    FuriString* selected_path;
} CfsApp;

// Worker events -> custom events use identity mapping (see enum above).
static inline uint32_t cfs_worker_event_to_custom(CfsNfcWorkerEvent event) {
    return (uint32_t)event;
}

// Shared submenu callback: forward the selected item's id as-is. Used by every
// scene whose submenu item ids already double as the custom event to send.
static inline void cfs_submenu_send_index(void* context, uint32_t index) {
    CfsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Stage app->pending (and every field the write wizard reads alongside it)
// from the currently loaded tag_data, ahead of a clone/edit write flow. Every
// entry point into the write wizard must set all of these fields together —
// missing one silently reuses stale state from an earlier flow.
static inline void cfs_app_stage_write_from_loaded(CfsApp* app, bool write_exact) {
    app->pending = app->tag_data;
    app->has_source_serial = true;
    app->dual_tag = false;
    app->write_exact = write_exact;
    app->pending_weight_index =
        cfs_nearest_weight_index(app->settings.weight_len, app->pending.length_m);
}

// Shared "operation finished" popup callback: return to the start menu.
static inline void cfs_app_popup_done_callback(void* context) {
    CfsApp* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CfsSceneStart);
}

// Show a result popup (header/text), buzz success/error, and auto-return to
// the start menu after a short timeout. Shared by the write and erase
// progress scenes.
static inline void
    cfs_app_finish_op_popup(CfsApp* app, const char* header, const char* text, bool success) {
    popup_reset(app->popup);
    popup_set_header(app->popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 34, AlignCenter, AlignCenter);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, cfs_app_popup_done_callback);
    popup_set_timeout(app->popup, 1600);
    popup_enable_timeout(app->popup);
    notification_message(app->notifications, success ? &sequence_success : &sequence_error);
}

// Persist the staged record under app->save_name and buzz the result. Shared by
// the save-name scene and the overwrite-confirm scene.
static inline bool cfs_app_save_pending(CfsApp* app) {
    uint8_t plain[CFS_PLAINTEXT_SIZE];
    bool ok;
    if(app->save_from_write) {
        // Staged write: no physical tag, so no UID. Encode the pending fields.
        cfs_data_encode(&app->pending, plain);
        ok = cfs_storage_save(app->storage, plain, NULL, 0, app->save_name);
    } else {
        // Read result / loaded record: save the exact captured plaintext when we
        // have it (lossless), else re-encode the parsed fields.
        if(app->has_tag_plain) {
            memcpy(plain, app->tag_plain, CFS_PLAINTEXT_SIZE);
        } else {
            cfs_data_encode(&app->tag_data, plain);
        }
        ok = cfs_storage_save(app->storage, plain, app->uid, app->uid_len, app->save_name);
    }
    notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    return ok;
}
