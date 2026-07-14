/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401LIGHTMSG_SCENE_BMPEDITOR_H
#define _401LIGHTMSG_SCENE_BMPEDITOR_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmp.h"
#include "401_err.h"
#include "drivers/sk6805.h"
#include <401_light_msg_icons.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/elements.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <lib/toolbox/name_generator.h>

typedef struct {
    uint8_t x;
    uint8_t y;
} bmpEditorCursor;

typedef enum {
    BmpEditorStateDrawing,
    BmpEditorStateSelectSize,
    BmpEditorStateSizeError,
    BmpEditorStateSelectName,
    BmpEditorStateSelectFile,
    BmpEditorStateSelectSource,
    BmpEditorStateMainMenu
} bmpEditorState;

// BMPEditor views
#define BMPEDITORVIEW_BASE 100
typedef enum {
    BmpEditorViewMainMenu = BMPEDITORVIEW_BASE,
    BmpEditorViewSelectSize,
    BmpEditorViewSelectName,
    BmpEditorViewDraw,
} BmpEditorViews;

typedef enum {
    BmpEditorMainmenuIndex_New,
    BmpEditorMainmenuIndex_Open,
} BmpEditorMainmenuIndex;

typedef enum {
    BmpEditorDrawModeOneshot,
    BmpEditorDrawModeContinuous,
} BmpEditorDrawMode;

typedef struct {
    bmpEditorCursor cursor;
    uint8_t bmp_pixel_size;
    uint8_t bmp_pixel_spacing;
    uint8_t bmp_w;
    uint8_t bmp_h;
    uint8_t bmp_frame_spacing;
    uint8_t bmp_frame_w;
    uint8_t bmp_frame_h;
    uint8_t bmp_frame_x;
    uint8_t bmp_frame_y;
    uint8_t* bmp_canvas;
    bitmapMatrix* bitmap;
    bmpEditorState state;
    BmpEditorDrawMode draw_mode;
    l401_err error;
} bmpEditorData;

typedef struct {
    bmpEditorData* data;
} bmpEditorModel;

typedef struct AppBmpEditor {
    View* view;
    bmpEditorData* model_data;
    Submenu* mainmenu;
    DialogsApp* dialogs;
    TextInput* text_input;
    char bitmapName[LIGHTMSG_MAX_BITMAPNAME_LEN + 1];
    char bitmapPath[LIGHTMSG_MAX_BITMAPPATH_LEN + 1];
} AppBmpEditor;

#include "401LightMsg_main.h"

typedef enum {
    AppBmpEditorEventQuit,
    AppBmpEditorEventSaveText,
} AppBmpEditorCustomEvents;

AppBmpEditor* app_bmp_editor_alloc(void* ctx);
void app_bmp_editor_free(void* ctx);
View* app_bitmap_editor_get_view(AppBmpEditor* appBmpEditor);
void app_scene_bmp_editor_on_enter(void* context);
bool app_scene_bmp_editor_on_event(void* context, SceneManagerEvent event);
void app_scene_bmp_editor_on_exit(void* context);

#endif
