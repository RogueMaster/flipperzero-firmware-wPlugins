#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>

#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <toolbox/path.h>

#include <infrared.h>
#include <infrared_signal.h>
#include <infrared_worker.h>
#include <infrared_transmit.h>

#include "ir_maker_draft.h"
#include "scenes/ir_maker_scene.h"

/* Remotes live in the same folder the stock Infrared app reads from, so a
 * remote built here shows up there directly. */
#define IR_MAKER_FOLDER EXT_PATH("infrared")
#define IR_MAKER_EXT    ".ir"

typedef enum {
    IrMakerViewSubmenu,
    IrMakerViewTextInput,
    IrMakerViewByteInput,
    IrMakerViewVarItemList,
    IrMakerViewDialogEx,
    IrMakerViewPopup,
} IrMakerView;

/* Which field the shared EditHex (ByteInput) scene is editing. */
typedef enum {
    IrMakerHexTargetDefaultAddress,
    IrMakerHexTargetButtonAddress,
    IrMakerHexTargetButtonCommand,
} IrMakerHexTarget;

/* What ButtonName should do on accept. New: add `pending` as a fresh button.
 * Signal: replace selected_button's signal (from `pending`) and name.
 * Rename: only rename selected_button, leaving its signal untouched. */
typedef enum {
    IrMakerEditModeNew,
    IrMakerEditModeSignal,
    IrMakerEditModeRename,
} IrMakerEditMode;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Storage* storage;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    VariableItemList* var_item_list;
    DialogEx* dialog_ex;
    Popup* popup;

    /* Owned by the Learn scene only: allocated in its on_enter, freed in its
     * on_exit. NULL at every other time so the IR HAL is never held idle. */
    InfraredWorker* worker;

    IrMakerDraft* draft;
    /* Staging slot for a signal being typed or learned before it is committed
     * into the draft. */
    InfraredSignal* pending;

    FuriString* file_path;

    /* Remote-wide defaults applied when seeding a new typed button. */
    InfraredProtocol def_protocol;
    uint32_t def_address;

    /* Working copy for the button currently being typed/edited. */
    InfraredMessage edit_msg;
    IrMakerEditMode edit_mode;

    /* Shared ByteInput state. hex_buf is edited MSB-first; hex_n_bytes is how
     * many of its bytes are live for the current field. */
    IrMakerHexTarget hex_target;
    uint8_t hex_buf[4];
    size_t hex_n_bytes;

    size_t selected_button;
    char name_buf[IR_MAKER_NAME_MAX];
} IrMakerApp;

/* ---- shared helpers (ir_maker.c) ---- */

/** Count of protocols that pass infrared_is_protocol_valid(). */
size_t ir_maker_valid_protocol_count(void);

/** The n-th valid protocol (0 <= index < ir_maker_valid_protocol_count()). */
InfraredProtocol ir_maker_valid_protocol_at(size_t index);

/** Position of `protocol` within the valid list, or 0 if not found. */
size_t ir_maker_valid_protocol_index(InfraredProtocol protocol);

/** Number of hex bytes (1..4) needed to hold a field of `bits` bits. */
size_t ir_maker_field_bytes(uint8_t bits);

/** Pack the low `n_bytes` of `value` into `buf`, most-significant byte first. */
void ir_maker_u32_to_hex_be(uint32_t value, uint8_t* buf, size_t n_bytes);

/** Read `n_bytes` of `buf` back to a uint32, most-significant byte first. */
uint32_t ir_maker_hex_be_to_u32(const uint8_t* buf, size_t n_bytes);

/** Clamp `msg`'s address/command to its protocol's field masks. */
void ir_maker_clamp_message(InfraredMessage* msg);

/** Transmit `signal` exactly once (parsed: protocol min-repeat frames; raw:
 * a single burst). Never repeats/holds. */
void ir_maker_send_test(const InfraredSignal* signal);

/** Fill `out` with a not-yet-used "<suggestion>.ir" path under IR_MAKER_FOLDER. */
void ir_maker_find_vacant_path(IrMakerApp* app, const char* suggestion, FuriString* out);
