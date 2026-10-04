#pragma once

#include <furi.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#define ARCHIVE_BROWSER_FAP_PATH EXT_PATH("apps/Main/archive_browser.fap")

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Intended to be called by settings apps to handle long presses, as well as
 * internally from within the archive
 * 
 * @param app_name name of the referring application
 * @param setting  name of the setting, which will be both displayed to the user
 *                 and passed to the application as an argument upon recall
 */
void archive_favorites_handle_setting_pin_unpin(const char* app_name, const char* setting);

bool process_favorite_launch(char** p);

typedef struct {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
} FavoriteTImeoutCtx;

void favorite_timeout_callback(void* _ctx);

void favorite_timeout_run(ViewDispatcher* view_dispatcher, SceneManager* scene_manager);

void run_with_default_app(const char* path);

/** Transfer a mounted image to resident ownership before a queued app launch.
 * Called only by Archive while the loader owns its active FAP. The loader
 * releases the image after the deferred launch queue finishes, even on error.
 */
void archive_handoff_disk_image(File* disk_image);

#ifdef __cplusplus
}
#endif
