#ifndef BUILD_HTTP_HEADERS_H
#define BUILD_HTTP_HEADERS_H

#include "../../app.h"

void scene_on_enter_build_http_headers(void* context);
void scene_on_exit_build_http_headers(void* context);
bool scene_on_event_build_http_headers(void* context, SceneManagerEvent event);
void draw_build_http_headers_menu(App* app);

#endif // BUILD_HTTP_HEADERS_H
