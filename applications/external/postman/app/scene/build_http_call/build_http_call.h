#ifndef BUILD_HTTP_CALL_H
#define BUILD_HTTP_CALL_H

#include "../../app.h"

void scene_on_enter_build_http_call(void* context);
void scene_on_exit_build_http_call(void* context);
bool scene_on_event_build_http_call(void* context, SceneManagerEvent event);
void draw_build_http_call_menu(App* app);

#endif // BUILD_HTTP_CALL_H
