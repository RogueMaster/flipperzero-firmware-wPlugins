#include "fmtx_app.h"

#include "fmtx_scenes.h"

#include <furi_hal.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

static bool custev(void* ctx, uint32_t event) {
    App* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool backev(void* ctx) {
    App* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void tickev(void* ctx) {
    App* app = ctx;
    scene_manager_handle_tick_event(app->scene_manager);
}

static bool startup_animation_due(void) {
    DateTime now;
    uint32_t today;
    uint32_t previous = 0;

    furi_hal_rtc_get_datetime(&now);
    today = ((uint32_t)now.year << 16) | ((uint32_t)now.month << 8) | now.day;
    if(saved_struct_load(
           APP_DATA_PATH("animation-day.bin"), &previous, sizeof(previous), 0x46, 1) &&
       previous == today)
        return false;
    (void)saved_struct_save(APP_DATA_PATH("animation-day.bin"), &today, sizeof(today), 0x46, 1);
    return true;
}

uint32_t fmtx_config_load_frequency(void) {
    uint8_t data[4];
    uint32_t hz = fmtx_vfo_default_frequency();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage ? storage_file_alloc(storage) : NULL;
    if(file &&
       storage_file_open(file, APP_DATA_PATH("config.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) == sizeof(data) &&
           storage_file_read(file, data, sizeof(data)) == sizeof(data)) {
            uint32_t saved_hz = data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                                ((uint32_t)data[3] << 24);
            if(fmtx_vfo_frequency_valid(saved_hz)) hz = saved_hz;
        }
        storage_file_close(file);
    }
    if(file) storage_file_free(file);
    if(storage) furi_record_close(RECORD_STORAGE);
    return hz;
}

bool fmtx_config_save_frequency(uint32_t hz) {
    uint8_t data[4] = {
        hz,
        hz >> 8,
        hz >> 16,
        hz >> 24,
    };
    bool ok = false;
    Storage* storage;
    File* file;
    FS_Error err;
    if(!fmtx_vfo_frequency_valid(hz)) return false;
    storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    err = storage_common_mkdir(storage, APP_DATA_PATH(""));
    file = storage_file_alloc(storage);
    if((err == FSE_OK || err == FSE_EXIST) && file &&
       storage_file_open(file, APP_DATA_PATH("config.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, data, sizeof(data)) == sizeof(data);
        storage_file_close(file);
    }
    if(file) storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static App* appnew(void) {
    App* app = calloc(1, sizeof(App));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->dispatcher = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->settings_menu = submenu_alloc();
    app->about_widget = widget_alloc();
    app->playback_view = view_alloc();
    app->vfo_view = view_alloc();
    app->playback = fmtx_playback_alloc();
    app->vfo = fmtx_vfo_alloc();
    app->path = furi_string_alloc_set(APP_ASSETS_PATH("0-chiptune.mp3"));
    app->frequency_hz = fmtx_config_load_frequency();
    if(app->gui && app->dialogs && app->dispatcher && app->menu && app->settings_menu &&
       app->about_widget && app->playback_view && app->vfo_view && app->playback && app->vfo &&
       app->path)
        app->scene_manager = scene_manager_alloc(&scenes, app);
    if(!app->scene_manager) {
        if(app->path) furi_string_free(app->path);
        fmtx_vfo_free(app->vfo);
        fmtx_playback_free(app->playback);
        if(app->vfo_view) view_free(app->vfo_view);
        if(app->playback_view) view_free(app->playback_view);
        if(app->about_widget) widget_free(app->about_widget);
        if(app->settings_menu) submenu_free(app->settings_menu);
        if(app->menu) submenu_free(app->menu);
        if(app->dispatcher) view_dispatcher_free(app->dispatcher);
        if(app->dialogs) furi_record_close(RECORD_DIALOGS);
        if(app->gui) furi_record_close(RECORD_GUI);
        free(app);
        return NULL;
    }

    view_allocate_model(app->playback_view, ViewModelTypeLocking, sizeof(PlayModel));
    view_set_draw_callback(app->playback_view, playdraw);
    view_set_input_callback(app->playback_view, playinput);
    view_set_context(app->playback_view, app);
    view_allocate_model(app->vfo_view, ViewModelTypeLocking, sizeof(FmtxVfoViewModel));
    FmtxVfoViewModel* m = view_get_model(app->vfo_view);
    m->vfo = app->vfo;
    view_commit_model(app->vfo_view, false);
    view_set_draw_callback(app->vfo_view, vfodraw);
    view_set_input_callback(app->vfo_view, vfoinput);
    view_set_context(app->vfo_view, app);
    widget_add_text_scroll_element(app->about_widget, 0, 0, 128, 52, abttext);
    widget_add_button_element(app->about_widget, GuiButtonTypeLeft, "Back", abtback, app);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custev);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, backev);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tickev, 100U);
    view_dispatcher_add_view(app->dispatcher, VMain, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->dispatcher, VPlay, app->playback_view);
    view_dispatcher_add_view(
        app->dispatcher, FmtxViewSettings, submenu_get_view(app->settings_menu));
    view_dispatcher_add_view(app->dispatcher, FmtxViewVfo, app->vfo_view);
    view_dispatcher_add_view(app->dispatcher, VAbout, widget_get_view(app->about_widget));
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void appfree(App* app) {
    if(!app) return;
    fmtx_playback_stop(app->playback);
    view_dispatcher_remove_view(app->dispatcher, VMain);
    view_dispatcher_remove_view(app->dispatcher, VPlay);
    view_dispatcher_remove_view(app->dispatcher, FmtxViewSettings);
    view_dispatcher_remove_view(app->dispatcher, FmtxViewVfo);
    view_dispatcher_remove_view(app->dispatcher, VAbout);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->dispatcher);
    submenu_free(app->menu);
    submenu_free(app->settings_menu);
    widget_free(app->about_widget);
    view_free(app->playback_view);
    view_free(app->vfo_view);
    fmtx_playback_free(app->playback);
    fmtx_vfo_free(app->vfo);
    furi_string_free(app->path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t flipper_zero_fmtx_app(void* ctx) {
    UNUSED(ctx);
    App* app = appnew();
    if(!app) return 255;
    scene_manager_next_scene(app->scene_manager, startup_animation_due() ? ScBoot : ScMain);
    view_dispatcher_run(app->dispatcher);
    appfree(app);

    return 0;
}
