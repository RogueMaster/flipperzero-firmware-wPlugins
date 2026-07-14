#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define HOT_COUNT_THRESHOLD_CLASSIC 4
#define HOT_COUNT_THRESHOLD_HIOPT   4
#define HOT_COUNT_THRESHOLD_HIOPT2  6
#define HOT_COUNT_THRESHOLD_OMEGA   4
#define HOT_COUNT_THRESHOLD_TRUE    4

typedef struct BlackjackCounterApp BlackjackCounterApp;
typedef struct BlackjackCounterView BlackjackCounterView;

typedef enum {
    BlackjackCounterViewMenu,
    BlackjackCounterViewCounter,
    BlackjackCounterViewCounter1,
    BlackjackCounterViewCounter2,
    BlackjackCounterViewCounter3,
    BlackjackCounterViewCounter4,
    BlackjackCounterViewCounter5,
    BlackjackCounterViewCounter6,
    BlackjackCounterViewCounter7,
    BlackjackCounterViewIndex,
    BlackjackCounterViewAbout,
} BlackjackCounterViewId;

typedef enum {
    BlackjackCounterSceneMenu,
    BlackjackCounterSceneHiLo,
    BlackjackCounterSceneHiOpt1,
    BlackjackCounterSceneHiOpt2,
    BlackjackCounterSceneOmega2,
    BlackjackCounterSceneTrueCount,
    BlackjackCounterSceneHiLoSide,
    BlackjackCounterSceneHiOpt1Side,
    BlackjackCounterSceneHiOpt2Side,
    BlackjackCounterSceneIndex,
    BlackjackCounterSceneAbout,
    BlackjackCounterSceneNum,
} BlackjackCounterScene;

typedef enum {
    BlackjackCounterCustomEventExit,
    BlackjackCounterCustomEventBack,
} BlackjackCounterCustomEvent;

typedef enum {
    BlackjackCounterMethodHiLo,
    BlackjackCounterMethodHiOpt1,
    BlackjackCounterMethodHiOpt2,
    BlackjackCounterMethodOmega2,
    BlackjackCounterMethodTrueCount,
    BlackjackCounterMethodHiLoSide,
    BlackjackCounterMethodHiOpt1Side,
    BlackjackCounterMethodHiOpt2Side,
} BlackjackCounterMethod;

struct BlackjackCounterApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    BlackjackCounterView* counter_views[8];
    View* index_view;
    View* about_view;
};

// Counter view
BlackjackCounterView* blackjack_counter_view_alloc();
void blackjack_counter_view_free(BlackjackCounterView* view);
View* blackjack_counter_view_get_view(BlackjackCounterView* view);
void blackjack_counter_view_set_method(BlackjackCounterView* view, BlackjackCounterMethod method);

// Scenes
extern const SceneManagerHandlers blackjack_counter_scene_handlers;

// Scene handlers
void blackjack_counter_scene_menu_on_enter(void* context);
bool blackjack_counter_scene_menu_on_event(void* context, SceneManagerEvent event);
void blackjack_counter_scene_menu_on_exit(void* context);

void blackjack_counter_scene_counter_on_enter(void* context);
bool blackjack_counter_scene_counter_on_event(void* context, SceneManagerEvent event);
void blackjack_counter_scene_counter_on_exit(void* context);
