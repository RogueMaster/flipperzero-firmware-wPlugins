#pragma once

#include "gemini_app.h"
#include "views/gemini_text_box.h"

#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>

#include "scenes/gemini_scene.h"
#include "helpers/uart_helper.h"

typedef struct GeminiApp GeminiApp;
struct GeminiApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    GeminiTextBox* gemini_text_box;
    TextInput* text_input;
    Widget* widget;
    FuriTimer* timer;

    UartHelper* uart_helper;
};
