#ifndef yo3gnd_fmtx_app_h
#define yo3gnd_fmtx_app_h

#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "fmtx_playback.h"
#include "fmtx_vfo.h"

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    ViewDispatcher* dispatcher;
    SceneManager* scene_manager;
    Submenu* menu;
    Submenu* settings_menu;
    Widget* about_widget;
    View* playback_view;
    View* vfo_view;
    Play* playback;
    FmtxVfo* vfo;
    FuriString* path;
    uint32_t frequency_hz;
    uint32_t screen_started;
    uint32_t pause_started;
    uint32_t hold_started;
    InputKey held_key;
    bool holding;
    bool hold_handled;
    bool playback_visible;
} App;

uint32_t fmtx_config_load_frequency(void);
bool fmtx_config_save_frequency(uint32_t hz);
int32_t flipper_zero_fmtx_app(void* ctx);

#endif
