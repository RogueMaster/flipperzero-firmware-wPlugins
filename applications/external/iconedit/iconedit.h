#pragma once

#include <gui/gui.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include "icon.h"

#define TAG             "IE"
#define APP_NAME        "IconEdit"
#define ENABLE_XBM_SAVE // Enables XBM file save

typedef struct {
    InputEvent input;
} IconEditEvent;

// Panel determines what should process input
typedef enum {
    // main view components
    Panel_TabBar,
    Panel_Canvas,

    Panel_File,
    Panel_Tools,
    Panel_Settings,
    Panel_Help,
    Panel_About,

    // modal panels
    Panel_Playback,
    Panel_SaveAs, // PNG, XBM, .C, BMX
    Panel_New, // dimension prompt
    Panel_FPS, // select FPS
    Panel_SendUSB,
    Panel_Dialog,
    Panel_SendAs,
} PanelType;

typedef void (*IconEditUpdateCallback)(void* context);

typedef enum {
    Setting_START,
    Setting_Canvas_Scale = Setting_START,
    Setting_Draw_Cursor_Guides,
    Setting_COUNT,
    Setting_NONE,
} SettingType;

#define SETTING_SCALE_AUTO     0u
#define SETTING_SCALE_AUTO_MIN 2u
typedef struct {
    size_t canvas_scale; // 0 is Auto
    bool draw_cursor_guides; // default is true
} IESettings;

typedef struct {
    IEIcon* icon;

    PanelType panel; // the active panel currently reacting to input
    IESettings settings;

    FuriMutex* mutex;
    Storage* storage;
    NotificationApp* notify;
    FuriString* temp_str;
    char temp_cstr[64];
    bool running;
    bool dirty;
} IconEdit;

void iconedit_load_settings(IconEdit* app);
void iconedit_save_settings(IconEdit* app);
