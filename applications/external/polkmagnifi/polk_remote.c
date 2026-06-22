#include "polk_remote.h"
#include "scenes.h"

// ── IR signal definitions ─────────────────────────────────────────────────────
// Bytes from Polk_MagnifiMini.ir are little-endian: "C8 91 00 00" → 0x000091C8
// All signals share the same address.

#define POLK_ADDR 0x000091C8u

static const InfraredMessage IR_POWER       = {InfraredProtocolNECext, POLK_ADDR, 0x0000FF00u, false};
static const InfraredMessage IR_VOL_UP      = {InfraredProtocolNECext, POLK_ADDR, 0x0000E11Eu, false};
static const InfraredMessage IR_VOL_DOWN    = {InfraredProtocolNECext, POLK_ADDR, 0x0000E01Fu, false};
static const InfraredMessage IR_MUTE        = {InfraredProtocolNECext, POLK_ADDR, 0x0000DF20u, false};
static const InfraredMessage IR_PLAY        = {InfraredProtocolNECext, POLK_ADDR, 0x0000DE21u, false};
static const InfraredMessage IR_NEXT        = {InfraredProtocolNECext, POLK_ADDR, 0x0000DB24u, false};
static const InfraredMessage IR_PREV        = {InfraredProtocolNECext, POLK_ADDR, 0x0000D926u, false};
static const InfraredMessage IR_INPUT_AUX   = {InfraredProtocolNECext, POLK_ADDR, 0x0000F20Du, false};
static const InfraredMessage IR_INPUT_OPT   = {InfraredProtocolNECext, POLK_ADDR, 0x0000F10Eu, false};
static const InfraredMessage IR_INPUT_HDMI  = {InfraredProtocolNECext, POLK_ADDR, 0x0000F00Fu, false};
static const InfraredMessage IR_VOICE_UP    = {InfraredProtocolNECext, POLK_ADDR, 0x0000956Au, false};
static const InfraredMessage IR_VOICE_DOWN  = {InfraredProtocolNECext, POLK_ADDR, 0x0000946Bu, false};
static const InfraredMessage IR_BASS_UP     = {InfraredProtocolNECext, POLK_ADDR, 0x0000A35Cu, false};
static const InfraredMessage IR_BASS_DOWN   = {InfraredProtocolNECext, POLK_ADDR, 0x0000A25Du, false};
static const InfraredMessage IR_MODE_CINEMA = {InfraredProtocolNECext, POLK_ADDR, 0x00008877u, false};
static const InfraredMessage IR_MODE_SPORTS = {InfraredProtocolNECext, POLK_ADDR, 0x00008679u, false};
static const InfraredMessage IR_MODE_MUSIC  = {InfraredProtocolNECext, POLK_ADDR, 0x00008778u, false};
static const InfraredMessage IR_MODE_NIGHT  = {InfraredProtocolNECext, POLK_ADDR, 0x00008E71u, false};

// ── Row data ──────────────────────────────────────────────────────────────────

static const char* const audio_mode_labels[] = {"Cinema", "Sports", "Music"};
static const InfraredMessage* const audio_mode_cmds[] = {
    &IR_MODE_CINEMA, &IR_MODE_SPORTS, &IR_MODE_MUSIC};

static const char* const input_labels[] = {"AUX", "Optical", "HDMI ARC"};
static const InfraredMessage* const input_cmds[] = {
    &IR_INPUT_AUX, &IR_INPUT_OPT, &IR_INPUT_HDMI};

const RemoteRow ROWS[ROWS_COUNT] = {
    {"POWER",    RowLOR,     &IR_MODE_NIGHT,&IR_POWER,     &IR_MUTE,      false, NULL,              NULL,            0},
    {"VOL",      RowLOR,     &IR_VOL_DOWN,  NULL,          &IR_VOL_UP,    false, NULL,              NULL,            0},
    {"PLAYBACK", RowLOR,     &IR_PREV,      &IR_PLAY,      &IR_NEXT,      false, NULL,              NULL,            0},
    {"VOICE",    RowLR,      &IR_VOICE_DOWN,NULL,          &IR_VOICE_UP,  false, NULL,              NULL,            0},
    {"BASS",     RowLR,      &IR_BASS_DOWN, NULL,          &IR_BASS_UP,   false, NULL,              NULL,            0},
    {"SOURCE",    RowSubmenu, NULL,         NULL,          NULL,          false, input_labels,      input_cmds,      3},
    {"EQ PRESET", RowSubmenu, NULL,        NULL,          NULL,          false, audio_mode_labels, audio_mode_cmds, 3},
};

// ── IR send ───────────────────────────────────────────────────────────────────

void polk_send(const InfraredMessage* msg) {
    if(!msg) return;
    infrared_send(msg, 2);
}

// ── Scene manager setup ───────────────────────────────────────────────────────

static const AppSceneOnEnterCallback on_enter_handlers[SceneCount] = {
    scene_main_on_enter,
    scene_submenu_on_enter,
};
static const AppSceneOnEventCallback on_event_handlers[SceneCount] = {
    scene_main_on_event,
    scene_submenu_on_event,
};
static const AppSceneOnExitCallback on_exit_handlers[SceneCount] = {
    scene_main_on_exit,
    scene_submenu_on_exit,
};
static const SceneManagerHandlers polk_scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers  = on_exit_handlers,
    .scene_num         = SceneCount,
};

static bool polk_custom_event_cb(void* ctx, uint32_t event) {
    PolkRemoteApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool polk_navigation_event_cb(void* ctx) {
    PolkRemoteApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

// ── App alloc / free ──────────────────────────────────────────────────────────

static PolkRemoteApp* polk_remote_alloc(void) {
    PolkRemoteApp* app = malloc(sizeof(PolkRemoteApp));

    app->scene_manager = scene_manager_alloc(&polk_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, polk_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, polk_navigation_event_cb);

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->main_view = view_alloc();
    view_set_context(app->main_view, app);
    view_set_orientation(app->main_view, ViewOrientationVertical);
    view_set_draw_callback(app->main_view, scene_main_draw_cb);
    view_set_input_callback(app->main_view, scene_main_input_cb);
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(MainViewModel));
    view_dispatcher_add_view(app->view_dispatcher, SceneMain, app->main_view);

    app->submenu_view = view_alloc();
    view_set_context(app->submenu_view, app);
    view_set_orientation(app->submenu_view, ViewOrientationVertical);
    view_set_draw_callback(app->submenu_view, scene_submenu_draw_cb);
    view_set_input_callback(app->submenu_view, scene_submenu_input_cb);
    view_allocate_model(app->submenu_view, ViewModelTypeLocking, sizeof(SubMenuViewModel));
    view_dispatcher_add_view(app->view_dispatcher, SceneSubMenu, app->submenu_view);

    app->selected_row = 0;
    app->active_row   = 0;

    scene_manager_next_scene(app->scene_manager, SceneMain);
    return app;
}

static void polk_remote_free(PolkRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, SceneMain);
    view_dispatcher_remove_view(app->view_dispatcher, SceneSubMenu);
    view_free(app->main_view);
    view_free(app->submenu_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    furi_record_close(RECORD_GUI);
    free(app);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int32_t polk_remote_app(void* p) {
    UNUSED(p);
    PolkRemoteApp* app = polk_remote_alloc();
    view_dispatcher_run(app->view_dispatcher);
    polk_remote_free(app);
    return 0;
}
