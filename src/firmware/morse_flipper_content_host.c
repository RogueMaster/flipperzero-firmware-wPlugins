#include "morse_flipper_app_i.h"

#include <flipper_application/plugins/plugin_manager.h>
#include <storage/storage.h>

#define MORSE_FLIPPER_CONTENT_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_help_about.fal")

bool morse_flipper_content_host_enter(
    MorseFlipperApp* app,
    MorseFlipperContentMode mode,
    uint8_t help_topic) {
    MorseFlipperContentEnterArgs args;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    args = (MorseFlipperContentEnterArgs){
        .mode = mode,
        .help_topic = help_topic,
        .now_ms = furi_get_tick(),
        .version = FAP_VERSION,
        .build_time = APP_BUILD_TIME,
        .build_commit = APP_BUILD_COMMIT,
        .build_host = APP_BUILD_HOST,
    };
    bool entered = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerContent,
        mode,
        MORSE_FLIPPER_CONTENT_PLUGIN_PATH,
        MORSE_FLIPPER_HELP_ABOUT_API_VERSION,
        MORSE_FLIPPER_HELP_ABOUT_API_MAGIC,
        sizeof(MorseFlipperHelpAboutApi),
        &args,
        NULL);
    furi_mutex_release(app->plugin_slot.mutex);
    return entered;
}

/* Caller holds plugin_mutex.  Navigation and unload happen after it releases. */
static MorseFlipperContentAction morse_flipper_content_host_apply_locked(
    MorseFlipperApp* app,
    MorseFlipperContentResult result) {
    if(result.help_topic_changed)
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuHelp, result.help_topic);
    if(result.action == MorseFlipperContentActionRedraw || result.redraw) morse_flipper_view_dirty(app);

    return result.action;
}

static void morse_flipper_content_host_apply_action(
    MorseFlipperApp* app,
    MorseFlipperContentAction action) {
    if(action == MorseFlipperContentActionBack) {
        morse_flipper_scene_back(app);
        morse_flipper_plugin_runtime_unload_current(app);
    } else if(action == MorseFlipperContentActionFinishOnboarding) {
        morse_flipper_onboarding_finish(app);
        morse_flipper_plugin_runtime_unload_current(app);
    } else if(action == MorseFlipperContentActionOpenTrace) {
        morse_flipper_plugin_runtime_unload_current(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuMain);
        morse_flipper_scene_open(app, MorseFlipperSceneTrace);
    }
}

bool morse_flipper_content_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperContentResult result = {0};
    MorseFlipperContentAction action;

    if(app == NULL || event == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerContent && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL)
        result = ((const MorseFlipperHelpAboutApi*)app->plugin_slot.api)
                     ->input(app->plugin_slot.state, event, now_ms);
    else {
        furi_mutex_release(app->plugin_slot.mutex);
        return false;
    }
    action = morse_flipper_content_host_apply_locked(app, result);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_content_host_apply_action(app, action);
    return true;
}

bool morse_flipper_content_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    morse_flipper_plugin_runtime_tick_locked(
        app, MorseFlipperPluginOwnerContent, now_ms, &result);
    furi_mutex_release(app->plugin_slot.mutex);
    if(result.redraw) morse_flipper_view_dirty(app);
    return result.redraw;
}

bool morse_flipper_onboarding_seen(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool seen = storage_file_open(file, MORSE_FLIPPER_ONBOARDING_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return seen;
}

void morse_flipper_onboarding_finish(MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    static const char marker[] = "seen\n";
    storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
    if(storage_file_open(file, MORSE_FLIPPER_ONBOARDING_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, marker, sizeof(marker) - 1U);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(app != NULL) {
        app->onboarding_seen = true;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, MorseFlipperSceneMenuMain);
    }
}
