#ifndef CONNECT_FAVS_H
#define CONNECT_FAVS_H

#include "../../app.h"
#include "../../structs.h"
#include "../../csv/csv_wifi/csv_wifi.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

// Function prototypes
void submenu_callback_select_fav_wifi(void* context, uint32_t index);
void submenu_callback_no_fav_wifi(void* context, uint32_t index);
void scene_on_enter_connect_favs(void* context);
bool scene_on_event_connect_favs(void* context, SceneManagerEvent event);
void scene_on_exit_connect_favs(void* context);

#endif // CONNECT_FAVS_H
