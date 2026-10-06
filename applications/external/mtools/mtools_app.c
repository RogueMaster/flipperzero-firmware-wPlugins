#include "mtools_app.h"
#include "features/uid_changer.h"
#include "features/about_ndef.h"
#include "features/magic_check.h"
#include "ui/mtools_ui.h"

#include <furi.h>
#include <gui/view.h>

#define MTOOLS_VIEW_MAIN 0
static bool mtools_view_input(InputEvent* event, void* context) {
    MToolsApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(app->active_scene == MToolsSceneAbout) {
        if(event->key == InputKeyDown && app->about_page == 0) {
            app->about_list_page = 1;
        } else if(event->key == InputKeyUp && app->about_page == 0) {
            app->about_list_page = 0;
        } else if(event->key == InputKeyRight && app->about_page == 0) {
            app->about_page = 1;
            mtools_about_ndef_start(app);
        } else if(event->key == InputKeyLeft && app->about_page == 1) {
            app->about_page = 0;
            mtools_about_ndef_stop(app);
        } else
            return false;
        view_commit_model(app->main_view, true);
        return true;
    }
    if(app->active_scene == MToolsSceneMagicType && event->key == InputKeyOk &&
       app->scan_status != 0 && app->scan_status != 6) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_RESCAN);
        return true;
    }
    if(app->active_scene != MToolsSceneHome) return false;
    if(event->key == InputKeyLeft || event->key == InputKeyRight || event->key == InputKeyUp ||
       event->key == InputKeyDown) {
        if(event->key == InputKeyUp || event->key == InputKeyLeft)
            app->selected_tool = (app->selected_tool + 2) % 3;
        else
            app->selected_tool = (app->selected_tool + 1) % 3;
        view_commit_model(app->main_view, true);
        return true;
    }
    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, app->selected_tool);
        return true;
    }
    return false;
}

static void mtools_scene_home_on_enter(void* context) {
    MToolsApp* app = context;
    app->active_scene = MToolsSceneHome;
    view_dispatcher_switch_to_view(app->view_dispatcher, MTOOLS_VIEW_MAIN);
    view_commit_model(app->main_view, true);
}

static bool mtools_scene_home_on_event(void* context, SceneManagerEvent event) {
    MToolsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event > 2) return false;
    static const MToolsScene scenes[] = {
        MToolsSceneMagicType, MToolsSceneUidChange, MToolsSceneAbout};
    scene_manager_next_scene(app->scene_manager, scenes[event.event]);
    return true;
}

static void mtools_scene_detail_on_enter(void* context) {
    MToolsApp* app = context;
    app->active_scene = scene_manager_get_current_scene(app->scene_manager);
    if(app->active_scene == MToolsSceneAbout) {
        app->about_page = 0;
        app->about_list_page = 0;
    }
    if(app->active_scene == MToolsSceneMagicType) {
        mtools_magic_check_start(app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, MTOOLS_VIEW_MAIN);
    view_commit_model(app->main_view, true);
}

static bool mtools_scene_type_on_event(void* context, SceneManagerEvent event) {
    if(event.type != SceneManagerEventTypeCustom) return false;
    return mtools_magic_check_event(context, event.event);
}

static void mtools_scene_type_on_exit(void* context) {
    MToolsApp* app = context;
    mtools_magic_check_stop(app);
}

static void mtools_scene_uid_on_enter(void* context) {
    MToolsApp* app = context;
    app->active_scene = MToolsSceneUidChange;
    mtools_uid_changer_enter(app->uid_changer);
}

static bool mtools_scene_uid_on_event(void* context, SceneManagerEvent event) {
    MToolsApp* app = context;
    return event.type == SceneManagerEventTypeCustom &&
           mtools_uid_changer_event(app->uid_changer, event.event);
}

static void mtools_scene_uid_on_exit(void* context) {
    MToolsApp* app = context;
    mtools_uid_changer_exit(app->uid_changer);
}

static void mtools_scene_no_exit(void* context) {
    UNUSED(context);
}

static bool mtools_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

static void mtools_scene_about_on_exit(void* context) {
    MToolsApp* app = context;
    if(app->about_listener || app->about_ndef_data) mtools_about_ndef_stop(app);
}

static void (*const scene_on_enter[])(void*) = {
    mtools_scene_home_on_enter,
    mtools_scene_detail_on_enter,
    mtools_scene_uid_on_enter,
    mtools_scene_detail_on_enter,
};
static bool (*const scene_on_event[])(void*, SceneManagerEvent) = {
    mtools_scene_home_on_event,
    mtools_scene_type_on_event,
    mtools_scene_uid_on_event,
    mtools_scene_about_on_event,
};
static void (*const scene_on_exit[])(void*) = {
    mtools_scene_no_exit,
    mtools_scene_type_on_exit,
    mtools_scene_uid_on_exit,
    mtools_scene_about_on_exit,
};

static const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = scene_on_enter,
    .on_event_handlers = scene_on_event,
    .on_exit_handlers = scene_on_exit,
    .scene_num = MToolsSceneCount,
};

static bool mtools_custom_event_callback(void* context, uint32_t event) {
    MToolsApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool mtools_back_event_callback(void* context) {
    MToolsApp* app = context;
    if(app->active_scene == MToolsSceneUidChange && mtools_uid_changer_back(app->uid_changer))
        return true;
    if(app->active_scene == MToolsSceneAbout && app->about_page == 1) {
        app->about_page = 0;
        mtools_about_ndef_stop(app);
        view_commit_model(app->main_view, true);
        return true;
    }
    if(app->active_scene != MToolsSceneHome) {
        scene_manager_handle_back_event(app->scene_manager);
    } else {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

int32_t mtools_app(void* p) {
    UNUSED(p);
    MToolsApp* app = malloc(sizeof(MToolsApp));
    if(!app) return -1;
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->nfc = nfc_alloc();
    app->scanner = nfc_scanner_alloc(app->nfc);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    app->magic_anim_timer =
        furi_timer_alloc(mtools_magic_check_timer_callback, FuriTimerTypePeriodic, app);
    app->uid_changer = mtools_uid_changer_alloc(app);
    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(MToolsApp*));
    *(MToolsApp**)view_get_model(app->main_view) = app;
    view_commit_model(app->main_view, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, mtools_ui_draw);
    view_set_input_callback(app->main_view, mtools_view_input);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, mtools_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, mtools_back_event_callback);
    view_dispatcher_add_view(app->view_dispatcher, MTOOLS_VIEW_MAIN, app->main_view);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MToolsSceneHome);
    view_dispatcher_run(app->view_dispatcher);

    mtools_uid_changer_free(app->uid_changer);
    furi_timer_stop(app->magic_anim_timer);
    furi_timer_free(app->magic_anim_timer);
    view_dispatcher_remove_view(app->view_dispatcher, MTOOLS_VIEW_MAIN);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    nfc_scanner_free(app->scanner);
    nfc_free(app->nfc);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
