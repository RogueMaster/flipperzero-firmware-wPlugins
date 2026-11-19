#pragma once

#include "subghz_editor_model.h"

void subghz_editor_view_init(SubGhzEditorApp* app);
void subghz_editor_view_refresh_list(SubGhzEditorApp* app);
void subghz_editor_show_message(SubGhzEditorApp* app, const char* title, const char* msg);
void subghz_editor_show_about(SubGhzEditorApp* app);
bool subghz_editor_custom_event_callback(void* context, uint32_t event);
