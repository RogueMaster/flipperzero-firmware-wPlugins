#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/menu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include "views/fas_list_view.h"

/* ── Filesystem paths ─────────────────────────────────────────────────── */
#define FAS_DOLPHIN_PATH        "/ext/dolphin"
#define FAS_PLAYLISTS_PATH      "/ext/apps_data/animation_switcher"
#define FAS_MANIFEST_PATH       "/ext/dolphin/manifest.txt"
#define FAS_MANIFEST_BACKUP_PATH "/ext/dolphin/manifest.txt.bak"

/* ── Limits ───────────────────────────────────────────────────────────── */
#define FAS_MAX_ANIMATIONS    128 // TODO Maybe increase this if possible?
#define FAS_ANIM_NAME_LEN      64
#define FAS_MAX_PLAYLISTS      64
#define FAS_PLAYLIST_NAME_LEN  32
#define FAS_PATH_LEN          160

/* ── Default animation values ─────────────────────────────────────────── */
#define FAS_DEFAULT_MIN_BUTTHURT  0
#define FAS_DEFAULT_MAX_BUTTHURT 14
#define FAS_DEFAULT_MIN_LEVEL     1
#define FAS_DEFAULT_MAX_LEVEL    30
#define FAS_DEFAULT_WEIGHT        3

/* ── Data structures ──────────────────────────────────────────────────── */
typedef struct {
    char name[FAS_ANIM_NAME_LEN];
    bool selected;
    int  min_butthurt;
    int  max_butthurt;
    int  min_level;
    int  max_level;
    int  weight;
} AnimEntry;

typedef struct {
    char name[FAS_PLAYLIST_NAME_LEN];
} PlaylistEntry;

/* ── View IDs ─────────────────────────────────────────────────────────── */
typedef enum {
    FasViewMenu,      /* Main menu (Menu module)          */
    FasViewList,      /* Custom FasListView               */
    FasViewVarList,   /* VariableItemList (anim settings) */
    FasViewTextInput, /* TextInput (playlist name)        */
    FasViewWidget,    /* Widget (about, preview)          */
    FasViewDialogEx,  /* DialogEx (confirmations)         */
} FasView;

/* ── Custom events sent through the view dispatcher ──────────────────── */
typedef enum {
    FasEvtAnimListOpenSettings = 0,
    FasEvtAnimListDone,
    FasEvtPlaylistNameDone,
    FasEvtChooseSelect,
    FasEvtChoosePreview,
    FasEvtDeleteSelect,
    FasEvtDeletePreview,
    FasEvtDeleteYes,
    FasEvtDeleteNo,
    FasEvtRebootYes,
    FasEvtRebootNo,
    FasEvtOverwriteYes,
    FasEvtOverwriteNo,
} FasCustomEvent;

/* ── Application context ──────────────────────────────────────────────── */
typedef struct {
    Gui*        gui;
    Storage*    storage;
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;

    /* Views */
    Menu*              menu;
    FasListView*       list_view;
    VariableItemList*  var_list;
    TextInput*         text_input;
    Widget*            widget;
    DialogEx*          dialog_ex;

    /* Animation data (loaded from /ext/dolphin) */
    AnimEntry animations[FAS_MAX_ANIMATIONS];
    int        animation_count;
    int        current_anim_index;   /* index of animation being edited */

    /* Playlist data (loaded from apps_data folder) */
    PlaylistEntry playlists[FAS_MAX_PLAYLISTS];
    int           playlist_count;
    int           current_playlist_index;

    /* TextInput work buffer */
    char text_input_buffer[FAS_PLAYLIST_NAME_LEN];

    /* Flag: true when returning to AnimList scene from AnimSettings */
    bool returning_from_settings;

    /* Flag: PlaylistName scene saves an imported manifest instead of the
     * currently-selected animations when true. */
    bool import_mode;
} FasApp;

/* ── Storage helpers (implemented in animation_switcher.c) ────── */
bool fas_load_animations(FasApp* app);
bool fas_load_playlists(FasApp* app);
bool fas_save_playlist(FasApp* app, const char* name);
bool fas_import_manifest(FasApp* app, const char* name);
bool fas_delete_playlist(FasApp* app, int index);
bool fas_apply_playlist(FasApp* app, int index);
bool fas_manifest_exists(FasApp* app);
bool fas_playlist_exists(FasApp* app, const char* name);
