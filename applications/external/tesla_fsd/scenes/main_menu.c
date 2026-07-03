#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"
#include <string.h>

#define SEND_TESTS_DIR "/ext/apps_data/tesla_mod/tests"

enum {
    MainMenuAutoDetect,
    MainMenuHW3,
    MainMenuHW4,
    MainMenuLegacy,
    MainMenuExtras,
    MainMenuSendTest,
    MainMenuSettings,
    MainMenuAbout,
};

static void main_menu_callback(void* context, uint32_t index) {
    TeslaFSDApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Open the file browser, load + parse a .cantest profile into app->send_steps.
// Returns true if at least one frame was parsed (then the caller enters send_run).
static bool load_send_profile(TeslaFSDApp* app) {
    storage_common_mkdir(app->storage, "/ext/apps_data/tesla_mod");
    storage_common_mkdir(app->storage, SEND_TESTS_DIR);

    FuriString* start = furi_string_alloc_set_str(SEND_TESTS_DIR);
    FuriString* result = furi_string_alloc();
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".cantest", NULL);
    opts.base_path = SEND_TESTS_DIR;

    bool loaded = false;
    if(dialog_file_browser_show(app->dialogs, result, start, &opts)) {
        File* f = storage_file_alloc(app->storage);
        if(storage_file_open(f, furi_string_get_cstr(result), FSAM_READ, FSOM_OPEN_EXISTING)) {
            char buf[4096];
            size_t n = storage_file_read(f, buf, sizeof(buf) - 1);
            buf[n] = '\0';
            storage_file_close(f);

            app->send_step_count = 0;
            app->send_blocked = 0;
            app->send_name[0] = '\0';
            // Split on '\n' manually (strtok_r is not in the FAP API).
            char* p = buf;
            while(*p && app->send_step_count < FSD_SEND_MAX_STEPS) {
                char* nl = strchr(p, '\n');
                if(nl) *nl = '\0';
                FsdProfileStep st;
                char nm[34];
                FsdProfileLineKind k = fsd_profile_parse_line(p, &st, nm, sizeof(nm));
                if(k == FSD_PLINE_STEP) {
                    app->send_steps[app->send_step_count++] = st;
                } else if(k == FSD_PLINE_BLOCKED) {
                    // safety-denied id (e.g. 0x229 right stalk): never load it
                    if(app->send_blocked < 0xFF) app->send_blocked++;
                } else if(k == FSD_PLINE_NAME) {
                    strncpy(app->send_name, nm, sizeof(app->send_name) - 1);
                    app->send_name[sizeof(app->send_name) - 1] = '\0';
                }
                if(!nl) break;
                p = nl + 1;
            }
            if(app->send_name[0] == '\0') {
                const char* full = furi_string_get_cstr(result);
                const char* base = strrchr(full, '/');
                strncpy(app->send_name, base ? base + 1 : full, sizeof(app->send_name) - 1);
                app->send_name[sizeof(app->send_name) - 1] = '\0';
            }
            loaded = (app->send_step_count > 0);
        }
        storage_file_free(f);
    }
    furi_string_free(result);
    furi_string_free(start);
    return loaded;
}

void tesla_fsd_scene_main_menu_on_enter(void* context) {
    TeslaFSDApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Tesla Mod");
    submenu_add_item(
        app->submenu, "Auto Detect & Start", MainMenuAutoDetect, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force HW3 Mode", MainMenuHW3, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force HW4 Mode", MainMenuHW4, main_menu_callback, app);
    submenu_add_item(app->submenu, "Force Legacy Mode", MainMenuLegacy, main_menu_callback, app);
    submenu_add_item(app->submenu, "Extras [BETA]", MainMenuExtras, main_menu_callback, app);
    submenu_add_item(app->submenu, "Send Test [BETA]", MainMenuSendTest, main_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", MainMenuSettings, main_menu_callback, app);
    submenu_add_item(app->submenu, "About", MainMenuAbout, main_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewSubmenu);
}

bool tesla_fsd_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MainMenuAutoDetect:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_hw_detect);
            consumed = true;
            break;
        case MainMenuHW3:
            app->hw_version = TeslaHW_HW3;
            fsd_state_init(&app->fsd_state, TeslaHW_HW3);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuHW4:
            app->hw_version = TeslaHW_HW4;
            fsd_state_init(&app->fsd_state, TeslaHW_HW4);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuLegacy:
            app->hw_version = TeslaHW_Legacy;
            fsd_state_init(&app->fsd_state, TeslaHW_Legacy);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
            break;
        case MainMenuExtras:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_extras);
            consumed = true;
            break;
        case MainMenuSendTest:
            // Blocking file browser → parse → enter the runner only if it loaded.
            if(load_send_profile(app)) {
                scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_send_run);
            }
            consumed = true;
            break;
        case MainMenuSettings:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_settings);
            consumed = true;
            break;
        case MainMenuAbout:
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_about);
            consumed = true;
            break;
        }
    }
    return consumed;
}

void tesla_fsd_scene_main_menu_on_exit(void* context) {
    TeslaFSDApp* app = context;
    submenu_reset(app->submenu);
}
