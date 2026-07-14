#ifndef DISPLAY_H
#define DISPLAY_H

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/text_box.h>

void scene_on_enter_display(void* context);
bool scene_on_event_display(void* context, SceneManagerEvent event);
void scene_on_exit_display(void* context);

#endif // DISPLAY_H
