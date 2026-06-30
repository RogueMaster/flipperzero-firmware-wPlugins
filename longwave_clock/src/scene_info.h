#ifndef SCENE_INFO_HEADERS
#define SCENE_INFO_HEADERS

#include "flipper.h"

void lwc_info_scene_on_enter(void* context);
bool lwc_info_scene_on_event(void* context, SceneManagerEvent event);
void lwc_info_scene_on_exit(void* context);

#endif
