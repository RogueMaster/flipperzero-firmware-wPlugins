#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#include "models/session.h"
#include "modules/storage_manager.h"
#include "modules/asset_manager.h"
#include "modules/graph_engine.h"
#include "modules/report_generator.h"
#include "views/graph_view.h"
#include "scenes/flipper_recon_scene.h"

/* Custom events. Submenu / variable item list callbacks send small indices,
 * so reserved events start high to avoid collisions. */
#define RECON_EVENT_TEXT_DONE   0x1000
#define RECON_EVENT_CONFIRM_YES 0x1001
#define RECON_EVENT_CONFIRM_NO  0x1002

typedef enum {
    ReconViewSubmenu,
    ReconViewTextInput,
    ReconViewVarItemList,
    ReconViewWidget,
    ReconViewGraph,
} ReconView;

typedef enum {
    ReconTextTargetSessionName,
    ReconTextTargetClient,
    ReconTextTargetLocation,
    ReconTextTargetAssetName,
    ReconTextTargetAssetNotes,
    ReconTextTargetEvidenceLabel,
    ReconTextTargetEvidencePath,
} ReconTextTarget;

typedef enum {
    ReconMessageAbout,
    ReconMessageInfo,
    ReconMessageConfirmDeleteSession,
    ReconMessageConfirmDeleteAsset,
    ReconMessageConfirmDeleteRelation,
    ReconMessageConfirmDeleteEvidence,
    ReconMessageConfirmOverwrite,
    ReconMessageConfirmDiscard,
} ReconMessageMode;

typedef struct FlipperReconApp FlipperReconApp;

struct FlipperReconApp {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* var_item_list;
    Widget* widget;
    GraphView* graph_view;

    Session* session; /* single allocation for the whole engagement */

    /* sanitized base name of the file this session is stored as (empty if the
     * session has never been saved). Used to handle rename and overwrite. */
    char session_file[RECON_NAME_LEN];

    /* scratch / navigation context */
    char text_buf[RECON_PATH_LEN];
    ReconTextTarget text_target;
    ReconMessageMode message_mode;
    FuriString* message_text;
    uint16_t selected_asset; /* index into session->assets */
    uint16_t selected_evidence; /* index into session->evidence */
    uint16_t selected_relation; /* index into session->relations */

    /* in-progress relation being created */
    uint16_t rel_from_id;
    uint16_t rel_to_id;
    bool rel_pick_to; /* false: choosing source, true: choosing target */

    /* cached session file names for the session list */
    FuriString* session_names[RECON_MAX_SESSION_FILES];
    size_t session_names_count;
};

/* Save the current session, handling rename (deletes the old file) and clearing
 * the dirty flag. Returns true on success. Fills message_text with the result. */
bool flipper_recon_perform_save(FlipperReconApp* app);
