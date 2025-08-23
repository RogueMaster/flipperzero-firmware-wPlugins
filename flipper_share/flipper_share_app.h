#pragma once

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/scene_manager.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <gui/modules/file_browser.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

#include "scenes/flipper_share_scene.h"

typedef struct FlipperShareApp FlipperShareApp;

typedef enum {
    FlipperShareViewIdMenu,
    FlipperShareViewIdFileBrowser,
    FlipperShareViewIdShowFile,
    FlipperShareViewIdReceive,
} FlipperShareViewId;

struct FlipperShareApp {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    FileBrowser* file_browser;
    DialogEx* dialog_show_file;
    DialogEx* dialog_receive;

    FuriString* result_path;
    char selected_file_path[256];
    size_t selected_file_size;
    bool file_info_loaded;
    
    void* file_reading_state;
    FuriTimer* timer;
};

void show_file_info_scene(FlipperShareApp* app); // Correct declaration
