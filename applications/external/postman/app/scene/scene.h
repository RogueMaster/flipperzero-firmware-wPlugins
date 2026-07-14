#ifndef SCENE_H
#define SCENE_H

#include "../app.h"
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

// Function declarations
void menu_callback_main_menu(void* context, uint32_t index);
void scene_on_enter_main_menu(void* context);
bool scene_on_event_main_menu(void* context, SceneManagerEvent event);
void scene_on_exit_main_menu(void* context);

bool scene_manager_custom_event_callback(void* context, uint32_t custom_event);
bool scene_manager_navigation_event_callback(void* context);

void scene_manager_init(App* app);
void view_dispatcher_init(App* app);

#endif
