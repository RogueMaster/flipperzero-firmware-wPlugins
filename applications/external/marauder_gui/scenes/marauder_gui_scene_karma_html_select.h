#pragma once

#include <gui/gui.h>

typedef struct MarauderGuiApp MarauderGuiApp;

void marauder_gui_scene_karma_html_select_on_enter(void* context);
bool marauder_gui_scene_karma_html_select_on_event(void* context, SceneManagerEvent event);
void marauder_gui_scene_karma_html_select_on_exit(void* context);

size_t marauder_gui_karma_html_file_count(void);
const char* marauder_gui_karma_html_filename(size_t index);
