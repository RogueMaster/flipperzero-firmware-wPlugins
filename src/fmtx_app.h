#ifndef yo3gnd_fmtx_app_h
#define yo3gnd_fmtx_app_h

#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "fmtx_playback.h"

typedef struct
{
    Gui *gui;
    DialogsApp *dlg;
    ViewDispatcher *vd;
    SceneManager *sm;
    Submenu *menu;
    View *pv;
    Play *play;
    FuriString *path;
    bool playing;
} App;

int32_t flipper_zero_fmtx_app(void *ctx);

#endif
