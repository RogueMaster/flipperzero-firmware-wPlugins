#ifndef SCENE_SUB_MENU_HEADERS
#define SCENE_SUB_MENU_HEADERS

#include "flipper.h"

void lwc_sub_menu_scene_on_enter(void* context);
bool lwc_sub_menu_scene_on_event(void* context, SceneManagerEvent event);
void lwc_sub_menu_scene_on_exit(void* context);

#endif
