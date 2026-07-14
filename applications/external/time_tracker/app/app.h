#include <gui/modules/dialog_ex.h>
#include <gui/modules/menu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include "structs.h"

// app/app.h
#ifndef APP_H
#define APP_H
#define KEY_NAME_SIZE   22
#define TEXT_STORE_SIZE 40

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Menu* menu;
    Submenu* submenu;
    Submenu* submenu_task_actions;
    View* view;
    Tasks* tasks;
    Task* current_task;
    File* file;
    DialogEx* dialog;
    FuriTimer* timer;
    NumberInput* number_input;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    char text_store[TEXT_STORE_SIZE + 1];
} App;

#endif // APP_H
