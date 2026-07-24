#include "morse_flipper_app_i.h"

#include <flipper_application/plugins/plugin_manager.h>
#include <storage/storage.h>

#define MORSE_FLIPPER_CONTENT_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_help_about.fal")

static bool morse_flipper_content_api_valid(const MorseFlipperHelpAboutApi* api) {
    return api != NULL && api->magic == MORSE_FLIPPER_HELP_ABOUT_API_MAGIC &&
           api->api_version == MORSE_FLIPPER_HELP_ABOUT_API_VERSION &&
           api->struct_size >= sizeof(MorseFlipperHelpAboutApi) && api->alloc != NULL &&
           api->free != NULL && api->enter != NULL && api->leave != NULL && api->input != NULL &&
           api->tick != NULL && api->draw != NULL;
}

void morse_flipper_content_host_unload_locked(MorseFlipperApp* app) {
    const MorseFlipperHelpAboutApi* api;
    void* state;
    PluginManager* manager;

    if(app == NULL || app->plugin_slot.owner != MorseFlipperPluginOwnerContent) return;
    api = app->plugin_slot.api;
    state = app->plugin_slot.state;
    manager = app->plugin_slot.manager;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    app->plugin_slot.manager = NULL;
    if(api != NULL && state != NULL) {
        api->leave(state);
        api->free(state);
    }
    if(manager != NULL) plugin_manager_free(manager);
    morse_flipper_plugin_runtime_release_claim_locked(app, MorseFlipperPluginOwnerContent);
}

void morse_flipper_content_host_unload(MorseFlipperApp* app) {
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    morse_flipper_content_host_unload_locked(app);
    furi_mutex_release(app->plugin_slot.mutex);
}

bool morse_flipper_content_host_enter(
    MorseFlipperApp* app,
    MorseFlipperContentMode mode,
    uint8_t help_topic) {
    PluginManager* manager = NULL;
    const MorseFlipperHelpAboutApi* api = NULL;
    void* state = NULL;
    MorseFlipperContentEnterArgs args;
    bool entered = false;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(!morse_flipper_plugin_runtime_claim_locked(app, MorseFlipperPluginOwnerContent, mode)) {
        furi_mutex_release(app->plugin_slot.mutex);
        return false;
    }
    app->plugin_slot.error = morse_flipper_plugin_runtime_load_locked(
        MORSE_FLIPPER_CONTENT_PLUGIN_PATH,
        MORSE_FLIPPER_HELP_ABOUT_API_VERSION,
        &manager,
        (const void**)&api);
    if(app->plugin_slot.error != MorseFlipperPluginErrorNone)
        goto cleanup;
    if(!morse_flipper_content_api_valid(api)) {
        app->plugin_slot.error = MorseFlipperPluginErrorTable;
        goto cleanup;
    }
    state = api->alloc();
    if(state == NULL) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    args = (MorseFlipperContentEnterArgs){
        .mode = mode,
        .help_topic = help_topic,
        .now_ms = furi_get_tick(),
        .version = FAP_VERSION,
        .build_time = APP_BUILD_TIME,
        .build_commit = APP_BUILD_COMMIT,
        .build_host = APP_BUILD_HOST,
    };
    if(!api->enter(state, &args)) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    entered = true;
    if(morse_flipper_plugin_runtime_publish_locked(
           app, MorseFlipperPluginOwnerContent, manager, api, state)) {
        furi_mutex_release(app->plugin_slot.mutex);
        return true;
    }
    app->plugin_slot.error = MorseFlipperPluginErrorState;

cleanup:
    if(entered && api != NULL && state != NULL) api->leave(state);
    if(api != NULL && state != NULL) api->free(state);
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->plugin_slot.mutex);
    return false;
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
        morse_flipper_content_host_unload(app);
    } else if(action == MorseFlipperContentActionFinishOnboarding) {
        morse_flipper_onboarding_finish(app);
        morse_flipper_content_host_unload(app);
    } else if(action == MorseFlipperContentActionOpenTrace) {
        morse_flipper_scene_open(app, MorseFlipperSceneTrace);
        morse_flipper_content_host_unload(app);
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
    bool redraw = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerContent && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL)
        redraw = ((const MorseFlipperHelpAboutApi*)app->plugin_slot.api)
                     ->tick(app->plugin_slot.state, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    if(redraw) morse_flipper_view_dirty(app);
    return redraw;
}

void morse_flipper_content_host_draw_unavailable(MorseFlipperApp* app, Canvas* canvas) {
    const char* title = "Help unavailable";
    const char* detail = "Plugin missing/corrupt";

    if(app->plugin_slot.mode == MorseFlipperContentModeOnboarding)
        title = "Setup unavailable";
    else if(app->plugin_slot.mode == MorseFlipperContentModeAbout)
        title = "About unavailable";

    if(app->plugin_slot.error == MorseFlipperPluginErrorHostId)
        detail = "Plugin host mismatch";
    else if(app->plugin_slot.error == MorseFlipperPluginErrorApiVersion)
        detail = "Plugin API mismatch";
    else if(app->plugin_slot.error == MorseFlipperPluginErrorTable)
        detail = "Plugin entry invalid";
    else if(app->plugin_slot.error == MorseFlipperPluginErrorState)
        detail = "Not enough memory";

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, detail);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back");
}

void morse_flipper_content_host_draw(MorseFlipperApp* app, Canvas* canvas) {
    if(app == NULL || canvas == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerContent && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL)
        ((const MorseFlipperHelpAboutApi*)app->plugin_slot.api)->draw(app->plugin_slot.state, canvas);
    else
        morse_flipper_content_host_draw_unavailable(app, canvas);
    furi_mutex_release(app->plugin_slot.mutex);
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
