#include "../cogs_mikai.h"

typedef enum {
    SubmenuIndexRead,
    SubmenuIndexInfo,
    SubmenuIndexWriteCard,
    SubmenuIndexAddCredit,
    SubmenuIndexSetCredit,
    SubmenuIndexReset,
    SubmenuIndexSaveFile,
    SubmenuIndexLoadFile,
    SubmenuIndexDebug,
    SubmenuIndexAbout,
} SubmenuIndex;

static void cogs_mikai_scene_start_submenu_callback(void* context, uint32_t index) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cogs_mikai_scene_start_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Submenu* submenu = app->submenu;

    // Always rebuild menu to reflect current state (e.g., is_modified flag)
    submenu_reset(submenu);

    if(app->mykey.is_loaded) {
        submenu_set_header(submenu, "[Card Loaded]");
    }

    submenu_add_item(
        submenu,
        "Read Card",
        SubmenuIndexRead,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "View Info",
        SubmenuIndexInfo,
        cogs_mikai_scene_start_submenu_callback,
        app);

    // Show "Write to Card" only if data has been modified
    if(app->mykey.is_modified) {
        submenu_add_item(
            submenu,
            ">>> Write to Card <<<",
            SubmenuIndexWriteCard,
            cogs_mikai_scene_start_submenu_callback,
            app);
    }

    submenu_add_item(
        submenu,
        "Add Credit", 
        SubmenuIndexAddCredit,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Set Credit",
        SubmenuIndexSetCredit,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Reset Card",
        SubmenuIndexReset,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Save to File",
        SubmenuIndexSaveFile,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Load from File",
        SubmenuIndexLoadFile,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "Debug Info",
        SubmenuIndexDebug,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_add_item(
        submenu,
        "About",
        SubmenuIndexAbout,
        cogs_mikai_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, COGSMyKaiSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewSubmenu);
}

bool cogs_mikai_scene_start_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, COGSMyKaiSceneStart, event.event);
        consumed = true;
        switch(event.event) {
            case SubmenuIndexRead:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneRead);
                break;
            case SubmenuIndexInfo:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneInfo);
                break;
            case SubmenuIndexWriteCard:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneWriteCard);
                break;
            case SubmenuIndexAddCredit:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneAddCredit);
                break;
            case SubmenuIndexSetCredit:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneSetCredit);
                break;
            case SubmenuIndexReset:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneReset);
                break;
            case SubmenuIndexSaveFile:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneSaveFile);
                break;
            case SubmenuIndexLoadFile:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneLoadFile);
                break;
            case SubmenuIndexDebug:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneDebug);
                break;
            case SubmenuIndexAbout:
                scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneAbout);
                break;
        }
    }

    return consumed;
}

void cogs_mikai_scene_start_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    submenu_reset(app->submenu);
}
