#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "cards_data.h"
#include "game.h"
#include "storage.h"

#define TAG "LoteriaZero"

typedef enum {
    LoteriaViewSubmenu,
    LoteriaViewWidget,
    LoteriaViewGame,
    LoteriaViewVariableItemList,
    LoteriaViewCount,
} LoteriaViewId;

typedef enum {
    LoteriaSceneWelcome,
    LoteriaSceneMainMenu,
    LoteriaSceneGame,
    LoteriaSceneHistory,
    LoteriaSceneStats,
    LoteriaSceneOurStory,
    LoteriaSceneSettings,
    LoteriaSceneAbout,
    LoteriaSceneAllCards,
    LoteriaSceneGameEnd,
    LoteriaSceneCount,
} LoteriaScene;

typedef enum {
    LoteriaCustomEventGameNext = 100,
    LoteriaCustomEventGamePrev,
    LoteriaCustomEventGameResumeCurrent,
    LoteriaCustomEventGameAutoTick,
    LoteriaCustomEventGameFinished,
} LoteriaCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    Widget* widget;
    VariableItemList* variable_item_list;
    View* game_view;

    LoteriaGame game;
    LoteriaPersist persist;

    uint8_t welcome_page;
    uint8_t all_cards_index;
    uint8_t history_offset;
    bool viewing_previous;
    int8_t rhyme_scroll;

    FuriTimer* auto_timer;
} LoteriaApp;

void loteria_app_save(LoteriaApp* app);
void loteria_auto_timer_apply(LoteriaApp* app);
void loteria_notify_card(LoteriaApp* app);
