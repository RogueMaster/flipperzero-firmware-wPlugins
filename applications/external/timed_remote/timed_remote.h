#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include "signal/infrared_signal.h"

typedef enum {
    MODE_COUNTDOWN,
    MODE_SCHEDULED,
} ModeId;

typedef enum {
    VIEW_MENU,
    VIEW_LIST,
    VIEW_RUN,
    VIEW_POP,
} ViewId;

#define SIGNAL_NAME_MAX_LEN 32
#define FILE_PATH_MAX_LEN   256

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    SceneManager* sm;

    Submenu* submenu;
    VariableItemList* vlist;
    Widget* widget;
    Popup* popup;

    InfraredSignal* ir;
    char sig[SIGNAL_NAME_MAX_LEN];
    char file[FILE_PATH_MAX_LEN];

    ModeId mode;
    uint8_t h;
    uint8_t m;
    uint8_t s;

    uint8_t repeat; /* 0 = off, 255 = unlimited, 1-99 = count */
    uint8_t repeat_left;

    FuriTimer* timer;
    uint32_t left;
} TimedRemoteApp;

TimedRemoteApp* timed_remote_app_alloc(void);
void timed_remote_app_free(TimedRemoteApp*);
