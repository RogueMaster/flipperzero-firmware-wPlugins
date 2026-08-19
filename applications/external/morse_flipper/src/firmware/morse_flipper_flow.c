/*
 * Purpose: Glue live view redraws, scene opening, and dispatcher transitions.
 * Owns: dirty signalling and scene/view activation helpers.
 * Depends on: morse_flipper_app_i.h and Flipper view dispatcher APIs.
 * Tests: firmware build; UI transitions are hardware-only.
 */

#include "morse_flipper_app_i.h"

void morse_flipper_view_dirty(MorseFlipperApp* app) {
    if(app == NULL || app->live_view == NULL) return;

    with_view_model(app->live_view, MorseFlipperLiveModel * m, { m->bump++; }, true);
}

void morse_flipper_live_draw(Canvas* canvas, void* model) {
    MorseFlipperLiveModel* m = model;

    if(m == NULL || m->app == NULL) return;
    morse_flipper_draw(canvas, m->app);
}

void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene) {
    if(app == NULL || app->scene_manager == NULL) return;
    scene_manager_next_scene(app->scene_manager, scene);
}

void morse_flipper_scene_back(MorseFlipperApp* app) {
    if(app == NULL || app->scene_manager == NULL) return;

    if(scene_manager_previous_scene(app->scene_manager)) return;

    scene_manager_stop(app->scene_manager);
    if(app->view_dispatcher) view_dispatcher_stop(app->view_dispatcher);
}

void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx) {
    MorseFlipperApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, idx);
}

static uint8_t morse_flipper_scene_screen(uint32_t scene) {
    switch(scene) {
    case MorseFlipperSceneRun:
        return MorseFlipperScreenRun;
    case MorseFlipperSceneRf:
        return MorseFlipperScreenRf;
    case MorseFlipperSceneRfRx:
        return MorseFlipperScreenRfRx;
    case MorseFlipperSceneRfFreq:
        return MorseFlipperScreenRfFreq;
    case MorseFlipperSceneSession:
        return MorseFlipperScreenSession;
    case MorseFlipperSceneStraight:
        return MorseFlipperScreenStraight;
    case MorseFlipperSceneSessionEnd:
        return MorseFlipperScreenSessionEnd;
    case MorseFlipperSceneTrace:
        return MorseFlipperScreenTrace;
    case MorseFlipperSceneHelp:
        return MorseFlipperScreenHelp;
    case MorseFlipperSceneAbout:
        return MorseFlipperScreenAbout;
    case MorseFlipperSceneStartupProbe:
        return MorseFlipperScreenStartupProbe;
    case MorseFlipperSceneHamRun:
        return MorseFlipperScreenHamRun;
    case MorseFlipperSceneHamStartRefusal:
        return MorseFlipperScreenHamStartRefusal;
    case MorseFlipperSceneHamAssign:
        return MorseFlipperScreenHamAssign;
    case MorseFlipperSceneHamAssignments:
        return MorseFlipperScreenHamAssignments;
    case MorseFlipperSceneHamCopyNotice:
        return MorseFlipperScreenHamCopyNotice;
    case MorseFlipperSceneHamDeleteConfirm:
        return MorseFlipperScreenHamDeleteConfirm;
    case MorseFlipperSceneTxGroups:
        return MorseFlipperScreenTxGroups;
    case MorseFlipperSceneTxGroupsResult:
        return MorseFlipperScreenTxGroupsResult;
    case MorseFlipperSceneTxGroupsFinal:
        return MorseFlipperScreenTxGroupsFinal;
    case MorseFlipperSceneOnboarding:
        return MorseFlipperScreenOnboarding;
    case MorseFlipperSceneProgress:
        return MorseFlipperScreenProgress;
    case MorseFlipperSceneStreakIntro:
        return MorseFlipperScreenStreakIntro;
    default:
        return MorseFlipperScreenMenu;
    }
}

static uint8_t morse_flipper_scene_view(uint32_t scene) {
    switch(scene) {
    case MorseFlipperSceneHome:
    case MorseFlipperSceneAudioCfg:
    case MorseFlipperSceneTrainer:
    case MorseFlipperSceneStraightCfg:
    case MorseFlipperSceneTxGroupsCfg:
    case MorseFlipperScenePc:
    case MorseFlipperSceneGpio:
        return MorseFlipperViewSettings;
    case MorseFlipperSceneHamTextInput:
        return MorseFlipperViewTextInput;
    case MorseFlipperSceneMenuMain:
    case MorseFlipperSceneMenuTraining:
    case MorseFlipperSceneMenuSettings:
    case MorseFlipperSceneMenuHelp:
    case MorseFlipperSceneMenuRf:
    case MorseFlipperSceneMenuHam:
    case MorseFlipperSceneHamConfigure:
    case MorseFlipperSceneHamMessageActions:
        return MorseFlipperViewMenu;
    default:
        return MorseFlipperViewLive;
    }
}

void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene) {
    uint8_t view = morse_flipper_scene_view(scene);

    morse_flipper_ensure_view(app, view);

    morse_flipper_enter_screen(app, morse_flipper_scene_screen(scene), scene, furi_get_tick());
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
    if(view == MorseFlipperViewLive) morse_flipper_view_dirty(app);
}
