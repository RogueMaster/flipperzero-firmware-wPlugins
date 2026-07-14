#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "../../app.h"
#include <furi.h>

// Function declarations
void menu_callback_main_menu(void* context, uint32_t index);
void scene_on_enter_main_menu(void* context);
bool scene_on_event_main_menu(void* context, SceneManagerEvent event);
void scene_on_exit_main_menu(void* context);

#endif // MAIN_MENU_H
