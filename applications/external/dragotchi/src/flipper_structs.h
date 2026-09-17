#ifndef __flipper_structs_h__
#define __flipper_structs_h__

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>

#include <gui/modules/loading.h>
#include "gui/custom_modules/button_panel.h" // Custom version of this module
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/submenu.h>

#include <core/thread.h>
#include <core/message_queue.h>
#include <core/timer.h>

#include "game_structs.h"

/* Application context structure */
struct ApplicationContext {
    /* GUI */
    Gui *gui;
    SceneManager *scene_manager;
    ViewDispatcher *view_dispatcher;
    /* Modules for GUI */
    Loading *loading_module;
    View *pet_view; // custom main view
    VariableItemList *variable_item_list_module;
    DialogEx *dialog_ex_module;
    TextBox *text_box_module;
    Submenu *menu_module;      // main menu
    Submenu *care_module;      // care submenu
    View *stats_view;          // custom Stats screen
    View *inventory_view;      // custom Inventory screen
    View *storm_view;          // custom animated Signal Storm screen

    /* Others */
    FuriTimer *storm_timer;    // animates the Signal Storm screen while open
    FuriThread *secondary_thread;
    FuriMessageQueue *threads_message_queue; // Message queue between main thread, GUI and secondary thread
    struct GameState *game_state; // Read by draw_callback thread, written by the secondary thread
};

#endif
