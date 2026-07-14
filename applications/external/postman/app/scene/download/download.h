#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "../../app.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef struct {
    int progress;
} DownloadProgressModel;

void scene_on_enter_download_progress(void* context);
void scene_on_exit_download_progress(void* context);
bool scene_on_event_download_progress(void* context, SceneManagerEvent event);
void update_download_progress(App* app, size_t progress, bool done);

#endif // DOWNLOAD_H
