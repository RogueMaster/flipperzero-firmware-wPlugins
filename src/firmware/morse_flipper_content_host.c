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

bool morse_flipper_content_host_init(MorseFlipperApp* app) {
    if(app == NULL) return false;
    app->content_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return app->content_mutex != NULL;
}

void morse_flipper_content_host_unload(MorseFlipperApp* app) {
    if(app == NULL || app->content_mutex == NULL) return;
    furi_mutex_acquire(app->content_mutex, FuriWaitForever);
    const MorseFlipperHelpAboutApi* api = app->content_api;
    void* state = app->content_state;
    PluginManager* manager = app->content_manager;
    app->content_api = NULL;
    app->content_state = NULL;
    app->content_manager = NULL;
    app->content_active = false;
    app->content_error = MorseFlipperContentErrorNone;
    if(api != NULL && state != NULL) {
        api->leave(state);
        api->free(state);
    }
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->content_mutex);
}

void morse_flipper_content_host_deinit(MorseFlipperApp* app) {
    if(app == NULL) return;
    morse_flipper_content_host_unload(app);
    if(app->content_mutex != NULL) {
        furi_mutex_free(app->content_mutex);
        app->content_mutex = NULL;
    }
}

bool morse_flipper_content_host_enter(
    MorseFlipperApp* app,
    MorseFlipperContentMode mode,
    uint8_t help_topic) {
    PluginManager* manager = NULL;
    const MorseFlipperHelpAboutApi* api = NULL;
    void* state = NULL;
    PluginManagerError error;
    MorseFlipperContentEnterArgs args;
    bool entered = false;

    if(app == NULL || app->content_mutex == NULL) return false;
    furi_mutex_acquire(app->content_mutex, FuriWaitForever);
    if(app->icr_active || app->content_active || app->content_manager != NULL || app->content_api != NULL ||
       app->content_state != NULL) {
        furi_mutex_release(app->content_mutex);
        return false;
    }
    app->content_mode = mode;
    app->content_error = MorseFlipperContentErrorLoad;
    manager = plugin_manager_alloc("morse_flipper", MORSE_FLIPPER_HELP_ABOUT_API_VERSION, NULL);
    error = plugin_manager_load_single(manager, MORSE_FLIPPER_CONTENT_PLUGIN_PATH);
    if(error != PluginManagerErrorNone || plugin_manager_get_count(manager) != 1U) {
        if(error == PluginManagerErrorApplicationIdMismatch)
            app->content_error = MorseFlipperContentErrorHostId;
        else if(error == PluginManagerErrorAPIVersionMismatch)
            app->content_error = MorseFlipperContentErrorApiVersion;
        goto cleanup;
    }
    api = plugin_manager_get_ep(manager, 0U);
    if(!morse_flipper_content_api_valid(api)) {
        app->content_error = MorseFlipperContentErrorTable;
        goto cleanup;
    }
    state = api->alloc();
    if(state == NULL) {
        app->content_error = MorseFlipperContentErrorState;
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
        app->content_error = MorseFlipperContentErrorState;
        goto cleanup;
    }
    entered = true;
    app->content_manager = manager;
    app->content_api = api;
    app->content_state = state;
    app->content_active = true;
    app->content_error = MorseFlipperContentErrorNone;
    furi_mutex_release(app->content_mutex);
    return true;

cleanup:
    if(entered && api != NULL && state != NULL) api->leave(state);
    if(api != NULL && state != NULL) api->free(state);
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->content_mutex);
    return false;
}

static void morse_flipper_content_host_apply(MorseFlipperApp* app, MorseFlipperContentResult result) {
    if(result.help_topic_changed)
        scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuHelp, result.help_topic);
    if(result.action == MorseFlipperContentActionRedraw || result.redraw) morse_flipper_view_dirty(app);
    if(result.action == MorseFlipperContentActionBack) {
        morse_flipper_scene_back(app);
        morse_flipper_content_host_unload(app);
    } else if(result.action == MorseFlipperContentActionFinishOnboarding) {
        morse_flipper_onboarding_finish(app);
        morse_flipper_content_host_unload(app);
    } else if(result.action == MorseFlipperContentActionOpenTrace) {
        morse_flipper_scene_open(app, MorseFlipperSceneTrace);
        morse_flipper_content_host_unload(app);
    }
}

bool morse_flipper_content_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperContentResult result = {0};
    if(app == NULL || event == NULL || app->content_mutex == NULL) return false;
    furi_mutex_acquire(app->content_mutex, FuriWaitForever);
    if(app->content_active && app->content_api != NULL && app->content_state != NULL)
        result = app->content_api->input(app->content_state, event, now_ms);
    else {
        furi_mutex_release(app->content_mutex);
        return false;
    }
    furi_mutex_release(app->content_mutex);
    morse_flipper_content_host_apply(app, result);
    return true;
}

bool morse_flipper_content_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    bool redraw = false;
    if(app == NULL || app->content_mutex == NULL) return false;
    furi_mutex_acquire(app->content_mutex, FuriWaitForever);
    if(app->content_active && app->content_api != NULL && app->content_state != NULL)
        redraw = app->content_api->tick(app->content_state, now_ms);
    furi_mutex_release(app->content_mutex);
    if(redraw) morse_flipper_view_dirty(app);
    return redraw;
}

void morse_flipper_content_host_draw_unavailable(MorseFlipperApp* app, Canvas* canvas) {
    const char* title = "Help unavailable";
    const char* detail = "Plugin missing/corrupt";

    if(app->content_mode == MorseFlipperContentModeOnboarding)
        title = "Setup unavailable";
    else if(app->content_mode == MorseFlipperContentModeAbout)
        title = "About unavailable";

    if(app->content_error == MorseFlipperContentErrorHostId)
        detail = "Plugin host mismatch";
    else if(app->content_error == MorseFlipperContentErrorApiVersion)
        detail = "Plugin API mismatch";
    else if(app->content_error == MorseFlipperContentErrorTable)
        detail = "Plugin entry invalid";
    else if(app->content_error == MorseFlipperContentErrorState)
        detail = "Not enough memory";

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, detail);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back");
}

void morse_flipper_content_host_draw(MorseFlipperApp* app, Canvas* canvas) {
    if(app == NULL || canvas == NULL || app->content_mutex == NULL) return;
    furi_mutex_acquire(app->content_mutex, FuriWaitForever);
    if(app->content_active && app->content_api != NULL && app->content_state != NULL)
        app->content_api->draw(app->content_state, canvas);
    else
        morse_flipper_content_host_draw_unavailable(app, canvas);
    furi_mutex_release(app->content_mutex);
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
