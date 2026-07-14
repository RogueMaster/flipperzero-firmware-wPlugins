#ifndef POST_H
#define POST_H

#include "../../app.h"

void scene_on_enter_post(void* context);
void scene_on_exit_post(void* context);
bool scene_on_event_post(void* context, SceneManagerEvent event);
void draw_post_menu(App* app);

#endif // POST_H
