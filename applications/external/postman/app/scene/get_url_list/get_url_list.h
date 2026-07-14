#ifndef GET_URL_LIST_H
#define GET_URL_LIST_H

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

void scene_on_enter_get_url_list(void* context);
bool scene_on_event_get_url_list(void* context, SceneManagerEvent event);
void scene_on_exit_get_url_list(void* context);

#endif // GET_URL_LIST_H
