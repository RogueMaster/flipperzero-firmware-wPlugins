#include "src/core/cfs_app_i.h"
#include <cfs_tag_writer_plus_icons.h>

// Ids are based off CfsCustomEventMenuBase (cfs_app_i.h), which sits past the
// worker-event range so a stray worker event queued during teardown can't alias
// a menu action (e.g. NotPresent=2 -> Erase).
typedef enum {
    StartItemRead = CfsCustomEventMenuBase,
    StartItemWrite,
    StartItemTemplates,
    StartItemErase,
    StartItemSaved,
    StartItemSettings,
    StartItemHelp,
    StartItemAbout,
} StartItem;

static const CfsMenuItem start_items[] = {
    {&I_MenuRead_10x10, "Read Tag"},
    {&I_MenuWrite_10x10, "Write Tag"},
    {&I_MenuTemplates_10x10, "Templates"},
    {&I_MenuErase_10x10, "Erase Tag"},
    {&I_MenuSaved_10x10, "Saved Tags"},
    {&I_MenuSettings_10x10, "Settings"},
    {&I_MenuHelp_10x10, "Help"},
    {&I_MenuAbout_10x10, "About"},
};

static const uint32_t start_events[] = {
    StartItemRead,
    StartItemWrite,
    StartItemTemplates,
    StartItemErase,
    StartItemSaved,
    StartItemSettings,
    StartItemHelp,
    StartItemAbout,
};

void cfs_scene_start_on_enter(void* context) {
    CfsApp* app = context;
    // Scene state holds the last chosen item id; reopen on it.
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, CfsSceneStart);
    size_t selected = state >= StartItemRead ? state - StartItemRead : 0;
    cfs_screen_show_menu(app->screen, start_items, start_events, COUNT_OF(start_items), selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewScreen);
}

bool cfs_scene_start_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, CfsSceneStart, event.event);
        switch(event.event) {
        case StartItemRead:
            scene_manager_next_scene(app->scene_manager, CfsSceneRead);
            return true;
        case StartItemWrite:
            // Fresh from-scratch write: seed defaults + configured brand.
            cfs_data_set_defaults(&app->pending);
            strncpy(
                app->pending.vendor,
                cfs_brand_to_vendor(app->settings.default_brand),
                sizeof(app->pending.vendor));
            app->dual_tag = false;
            app->write_exact = false;
            app->has_source_serial = false;
            app->pending_weight_index = 0;
            scene_manager_next_scene(app->scene_manager, CfsSceneWriteBrand);
            return true;
        case StartItemTemplates:
            scene_manager_next_scene(app->scene_manager, CfsSceneTemplates);
            return true;
        case StartItemErase:
            scene_manager_next_scene(app->scene_manager, CfsSceneEraseConfirm);
            return true;
        case StartItemSaved:
            scene_manager_next_scene(app->scene_manager, CfsSceneSavedTags);
            return true;
        case StartItemSettings:
            scene_manager_next_scene(app->scene_manager, CfsSceneSettings);
            return true;
        case StartItemHelp:
            scene_manager_next_scene(app->scene_manager, CfsSceneHelp);
            return true;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, CfsSceneAbout);
            return true;
        default:
            break;
        }
    }
    return false;
}

void cfs_scene_start_on_exit(void* context) {
    CfsApp* app = context;
    cfs_screen_reset(app->screen);
}
