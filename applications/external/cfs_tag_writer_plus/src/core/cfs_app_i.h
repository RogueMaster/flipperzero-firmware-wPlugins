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
#include "src/views/cfs_screen.h"

typedef enum {
    CfsViewSubmenu,
    CfsViewWidget,
    CfsViewPopup,
    CfsViewNumberInput,
    CfsViewByteInput,
    CfsViewTextInput,
    CfsViewVariableItemList,
    CfsViewScreen, // the app's own screens (src/views/cfs_screen.c)
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
    CfsCustomEventPickerPick, // color picker: OK on a shade / basic color
    CfsCustomEventPickerHex, // color picker: OK on the custom hex page
    CfsCustomEventScreenDone, // a finished operation's screen timed out

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
    CfsScreen* screen;

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
    bool save_as_template; // save into CFS_TEMPLATE_DIR instead of the saved-tags folder

    // Templates scene: names listed from CFS_TEMPLATE_DIR.
    char template_names[CFS_TEMPLATES_MAX][CFS_STORAGE_NAME_MAX];
    size_t template_count;

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

// Folder the save flow writes to: templates or saved tags.
static inline const char* cfs_app_save_dir(const CfsApp* app) {
    return app->save_as_template ? CFS_TEMPLATE_DIR : CFS_STORAGE_DIR;
}

// The scene the save flow returns to once the record is saved.
static inline CfsScene cfs_app_save_invoker(const CfsApp* app) {
    return app->save_from_write ? CfsSceneWriteConfirm : CfsSceneReadResult;
}

// Set app->pending.serial for a new write per the configured serial mode.
// Returns false when the mode asks the user (CfsSerialPrompt): the caller then
// opens the serial input scene. Generated once per write, so a 2x write puts
// the same serial on both tags.
static inline bool cfs_app_assign_serial(CfsApp* app) {
    switch(app->settings.serial_mode) {
    case CfsSerialPrompt:
        return false;
    case CfsSerialSequential:
        snprintf(
            app->pending.serial,
            sizeof(app->pending.serial),
            "%06lu",
            (unsigned long)(app->settings.serial_counter % 1000000UL));
        break;
    case CfsSerialRandom:
        cfs_serial_random(app->pending.serial);
        break;
    case CfsSerialKeepSource:
    default:
        // Keep the source serial on clone/edit; random when writing from scratch.
        if(!app->has_source_serial) cfs_serial_random(app->pending.serial);
        break;
    }
    return true;
}

// Persist the staged record under app->save_name and buzz the result. Shared by
// the save-name scene and the overwrite-confirm scene.
static inline bool cfs_app_save_pending(CfsApp* app) {
    uint8_t plain[CFS_PLAINTEXT_SIZE];
    bool ok;
    if(app->save_from_write) {
        // Staged write: no physical tag, so no UID. Encode the pending fields.
        cfs_data_encode(&app->pending, plain);
        ok = cfs_storage_save_in(
            app->storage, cfs_app_save_dir(app), plain, NULL, 0, app->save_name);
    } else {
        // Read result / loaded record: save the exact captured plaintext when we
        // have it (lossless), else re-encode the parsed fields.
        if(app->has_tag_plain) {
            memcpy(plain, app->tag_plain, CFS_PLAINTEXT_SIZE);
        } else {
            cfs_data_encode(&app->tag_data, plain);
        }
        // A template has no tag of its own, so it keeps no UID.
        ok = app->save_as_template ?
                 cfs_storage_save_in(
                     app->storage, CFS_TEMPLATE_DIR, plain, NULL, 0, app->save_name) :
                 cfs_storage_save(app->storage, plain, app->uid, app->uid_len, app->save_name);
    }
    notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    return ok;
}
