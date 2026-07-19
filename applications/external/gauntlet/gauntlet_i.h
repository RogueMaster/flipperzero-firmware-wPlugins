#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "gauntlet_icons.h" // generated from icons/ by fbt

#include "helpers/ctf_pack.h"
#include "helpers/ctf_store.h"
#include "helpers/ctf_codec.h"
#include "views/challenge_view.h"
#include "views/result_view.h"
#include "views/toolkit_view.h"
#include "scenes/gauntlet_scene.h"

#define GAUNTLET_VERSION      "1.0"
#define GAUNTLET_HINT_PENALTY 25
#define GAUNTLET_FLAG_LEN     64

typedef enum {
    GauntletViewSubmenu, // start / packs / browse
    GauntletViewTextInput, // flag entry / toolkit input
    GauntletViewVarList, // settings
    GauntletViewWidget, // progress / about / empty-state
    GauntletViewChallenge,
    GauntletViewResult,
    GauntletViewToolkit,
} GauntletViewId;

typedef enum {
    GauntletEventChallengeFlag = 100, // OK on a challenge -> enter flag
    GauntletEventChallengeTools, // Right on a challenge -> Toolkit
    GauntletEventChallengeHint, // reveal the hint
    GauntletEventResultContinue, // leave the result screen
    GauntletEventTextCommit, // flag entered in the solve text input
    GauntletEventToolCommit, // text entered for the toolkit
} GauntletCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;

    /* shared view modules */
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* var_item_list;
    Widget* widget;

    /* showpiece views */
    ChallengeView* challenge_view;
    ResultView* result_view;
    ToolkitView* toolkit_view;

    /* shared text buffer (flag entry / toolkit manual input) */
    char text_buf[GAUNTLET_FLAG_LEN];

    /* settings */
    bool sound;
    bool vibro;
    bool led;
    bool hints_free; // false => using a hint costs GAUNTLET_HINT_PENALTY

    /* library (heap-allocated to keep the struct light) */
    CtfPackInfo* packs; // [CTF_MAX_PACKS]
    uint8_t pack_count;
    uint8_t sel_pack;

    CtfPack* pack; // currently opened pack
    uint8_t sel_challenge;
    bool hint_shown; // hint revealed on the current challenge
    bool hint_used; // hint was revealed before solving (for penalty)

    CtfProgress* progress;

    /* result screen */
    bool last_correct;
    uint16_t last_points;

    /* toolkit */
    char tool_src[CTF_DATA_LEN];
    bool tool_from_challenge;
} GauntletApp;

/* current challenge shortcut (NULL-safe at call sites) */
static inline CtfChallenge* gauntlet_current_challenge(GauntletApp* app) {
    if(!app->pack || app->sel_challenge >= app->pack->count) return NULL;
    return &app->pack->challenges[app->sel_challenge];
}

/* feedback, all gated by settings (defined in gauntlet.c) */
void gauntlet_notify_correct(GauntletApp* app);
void gauntlet_notify_wrong(GauntletApp* app);
void gauntlet_notify_blip(GauntletApp* app);
