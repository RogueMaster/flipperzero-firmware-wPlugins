#include "flippass_scene_file_browser.h"
#include "../flippass.h"
#include "../plugins/flippass_file_browser_plugin.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"

/* App owns the directory and selection. The module owns only its current item
 * array and active-view callbacks, which on_exit detaches before the last pin
 * can return. A scene remaining in history does not retain those callbacks. */
static bool file_browser_unavailable;
static bool file_browser_enter_pending;

static const FlipPassFileBrowserPluginV1* file_browser_get(App* app, bool load) {
    const FlipperAppPluginDescriptor* descriptor =
        load ? flippass_module_ensure(
                   app,
                   FlipPassModuleSlotFileBrowser,
                   NULL,
                   FLIPPASS_FILE_BROWSER_PLUGIN_APP_ID,
                   FLIPPASS_FILE_BROWSER_PLUGIN_API_VERSION,
                   NULL) :
               app->module_loader.slot[FlipPassModuleSlotFileBrowser].descriptor;
    const FlipPassFileBrowserPluginV1* plugin = descriptor ? descriptor->entry_point : NULL;
    if(plugin == NULL || plugin->api_version != FLIPPASS_FILE_BROWSER_PLUGIN_API_VERSION ||
       plugin->on_enter == NULL || plugin->on_event == NULL || plugin->on_exit == NULL ||
       plugin->check_database == NULL) {
        flippass_module_unload(app, FlipPassModuleSlotFileBrowser);
        return NULL;
    }
    flippass_module_pin(app, FlipPassModuleSlotFileBrowser);
    return plugin;
}

static void file_browser_leave(App* app) {
    flippass_module_unpin(app, FlipPassModuleSlotFileBrowser);
    if(!app->module_loader.slot[FlipPassModuleSlotFileBrowser].sticky)
        flippass_module_unload(app, FlipPassModuleSlotFileBrowser);
}

bool flippass_file_browser_check_database(App* app, const char* path) {
    const FlipPassFileBrowserPluginV1* plugin = file_browser_get(app, true);
    if(plugin == NULL) return false;
    const bool ok = plugin->check_database(app, path);
    file_browser_leave(app);
    return ok;
}

void flippass_scene_file_browser_on_enter(void* context) {
    App* app = context;
    file_browser_unavailable = false;
    file_browser_enter_pending = false;
    if(app->module_loader.slot[FlipPassModuleSlotEditor].pin_count != 0U ||
       app->module_loader.slot[FlipPassModuleSlotSaveHost].pin_count != 0U ||
       app->module_loader.slot[FlipPassModuleSlotLaunch].pin_count != 0U) {
        /* Scene changes nest inside the outgoing handler. Its final pin cannot
         * release until it returns, so list files on a later dispatcher tick.
         * Posting a custom event here could block on the outgoing queue.
         * Launch also needs a picker history parent, without building a list
         * that would be discarded immediately by the password scene. */
        file_browser_enter_pending = true;
        return;
    }
    const FlipPassFileBrowserPluginV1* plugin = file_browser_get(app, true);
    if(plugin == NULL) {
        file_browser_unavailable = true;
        flippass_scene_status_show(
            app,
            "Browser Unavailable",
            "The file browser module could not load. Back exits.",
            FlipPassScene_FileBrowser);
        flippass_scene_status_on_enter(app);
        return;
    }
    flippass_module_set_sticky(app, FlipPassModuleSlotFileBrowser, true);
    plugin->on_enter(app);
    file_browser_leave(app);
}

bool flippass_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(file_browser_enter_pending) {
        if(event.type == SceneManagerEventTypeBack) {
            file_browser_enter_pending = false;
            flippass_request_exit(app);
        } else if(event.type == SceneManagerEventTypeTick) {
            if(app->module_loader.slot[FlipPassModuleSlotEditor].pin_count == 0U &&
               app->module_loader.slot[FlipPassModuleSlotSaveHost].pin_count == 0U &&
               app->module_loader.slot[FlipPassModuleSlotLaunch].pin_count == 0U) {
                flippass_scene_file_browser_on_enter(app);
            }
        }
        return true;
    }
    if(file_browser_unavailable) {
        if(event.type == SceneManagerEventTypeBack) {
            flippass_request_exit(app);
            return true;
        }
        return false;
    }
    const FlipPassFileBrowserPluginV1* plugin = file_browser_get(app, false);
    if(plugin == NULL) return false;
    const bool consumed = plugin->on_event(app, event);
    file_browser_leave(app);
    return consumed;
}

void flippass_scene_file_browser_on_exit(void* context) {
    App* app = context;
    file_browser_enter_pending = false;
    const FlipPassFileBrowserPluginV1* plugin = file_browser_get(app, false);
    if(plugin == NULL) return;
    plugin->on_exit(app);
    flippass_module_set_sticky(app, FlipPassModuleSlotFileBrowser, false);
    file_browser_leave(app);
}

void flippass_file_browser_shutdown(struct App* app) {
    /* No ensure/load during teardown. Active-picker app exit must release its
     * module-owned item array before the loader or shared views disappear. */
    flippass_scene_file_browser_on_exit(app);
    file_browser_unavailable = false;
}
