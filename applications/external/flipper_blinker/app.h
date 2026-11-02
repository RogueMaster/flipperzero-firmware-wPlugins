#pragma once

#include <furi.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>

#include "settings.h"
#include "views.h"

#define FLIPPER_BLINKER_TEXT_SIZE (128U)

typedef struct FlipperBlinkerApp FlipperBlinkerApp;

typedef enum {
    FlipperBlinkerViewMainMenu,
    FlipperBlinkerViewTextInput,
    FlipperBlinkerViewSettings,
    FlipperBlinkerViewNumberInput,
    FlipperBlinkerViewWidget,
    FlipperBlinkerViewTransmit,
} FlipperBlinkerView;

typedef enum {
    FlipperBlinkerMenuSetup,
    FlipperBlinkerMenuSendMessage,
    FlipperBlinkerMenuSettings,
    FlipperBlinkerMenuAbout,
} FlipperBlinkerMenuItem;

typedef enum {
    FlipperBlinkerEventSetupTestDone,
    FlipperBlinkerEventSetupRepeatTest,
    FlipperBlinkerEventTransmissionDone,
    FlipperBlinkerEventTransmissionStopped,
} FlipperBlinkerEvent;

struct FlipperBlinkerApp {
    ViewDispatcher* view_dispatcher;
    Submenu* main_menu;
    TextInput* text_input;
    NumberInput* number_input;
    VariableItemList* settings_list;
    Widget* widget;
    FlipperBlinkerTransmitView* transmit_view;
    FlipperBlinkerSettings settings;
    char text[FLIPPER_BLINKER_TEXT_SIZE];
    FuriThread* worker_thread;
    volatile bool worker_stop;
    FlipperBlinkerView current_view;
};

FlipperBlinkerApp* flipper_blinker_app_alloc(void);
void flipper_blinker_app_free(FlipperBlinkerApp* app);
void flipper_blinker_show_main_menu(FlipperBlinkerApp* app);
void flipper_blinker_start_setup_test(FlipperBlinkerApp* app);
void flipper_blinker_start_transmission(FlipperBlinkerApp* app);
void flipper_blinker_stop_transmission(FlipperBlinkerApp* app);
