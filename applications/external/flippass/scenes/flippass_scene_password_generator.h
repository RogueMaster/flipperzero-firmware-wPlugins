#pragma once

#include "../flippass_password_gen.h"

#include <gui/scene_manager.h>
#include <stdbool.h>

struct App;

void flippass_password_generator_prepare(struct App* app, FlipPassPasswordGenTarget target);

void flippass_scene_password_generator_on_enter(void* context);
bool flippass_scene_password_generator_on_event(void* context, SceneManagerEvent event);
void flippass_scene_password_generator_on_exit(void* context);
