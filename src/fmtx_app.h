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

typedef struct
{
    Gui *gui;
    DialogsApp *dlg;
    ViewDispatcher *vd;
    SceneManager *sm;
    Submenu *menu;
    Submenu *setmenu;
    Widget *abt;
    View *pv;
    View *vv;
    Play *play;
    FmtxVfo *vfo;
    FuriString *path;
    uint32_t hz;
    uint32_t spat;
    uint32_t pauseat;
    uint32_t holdat;
    InputKey holdkey;
    bool holding;
    bool heldskip;
    bool playing;
} App;

uint32_t cfgload(void);
bool cfgsave(uint32_t hz);
int32_t flipper_zero_fmtx_app(void *ctx);

#endif
