#ifndef CONNECT_H
#define CONNECT_H

#include "../../app.h"
#include <furi.h>
#include <gui/modules/submenu.h>

void submenu_callback_select_wifi(void* context, uint32_t index);
void submenu_callback_no_wifi(void* context, uint32_t index);
void scene_on_enter_connect(void* context);
bool scene_on_event_connect(void* context, SceneManagerEvent event);
void scene_on_exit_connect(void* context);
bool connect_init(App* app);

#endif // CONNECT_H
