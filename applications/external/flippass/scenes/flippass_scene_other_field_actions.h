#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <gui/scene_manager.h>

struct App;

void flippass_other_field_begin_type_action(struct App* app, bool bluetooth, uint32_t run_event);
void flippass_other_field_show_selected_value(struct App* app, uint32_t return_scene);
bool flippass_other_field_refresh_visible_show(struct App* app, uint32_t return_scene);
bool flippass_other_field_show_refresh_due(const struct App* app);
void flippass_other_field_clear_show_state(struct App* app);
void flippass_other_field_run_pending_type_action(struct App* app, uint32_t failure_return_scene);

void flippass_scene_other_field_actions_on_enter(void* context);
bool flippass_scene_other_field_actions_on_event(void* context, SceneManagerEvent event);
void flippass_scene_other_field_actions_on_exit(void* context);
