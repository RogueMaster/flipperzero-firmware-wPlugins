#pragma once

#include "../flippass_password_gen.h"

#include <gui/scene_manager.h>
#include <input/input.h>
#include <stdbool.h>

struct App;

void flippass_password_generator_prepare(struct App* app, FlipPassPasswordGenTarget target);
void flippass_password_generator_input_event(struct App* app, const InputEvent* event);

void flippass_scene_password_generator_on_enter(void* context);
bool flippass_scene_password_generator_on_event(void* context, SceneManagerEvent event);
void flippass_scene_password_generator_on_exit(void* context);

void flippass_scene_password_generator_harvest_on_enter(void* context);
bool flippass_scene_password_generator_harvest_on_event(void* context, SceneManagerEvent event);
void flippass_scene_password_generator_harvest_on_exit(void* context);
