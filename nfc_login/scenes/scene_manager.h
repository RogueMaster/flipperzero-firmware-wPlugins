#pragma once

#include "nfc_login_app.h"
#include <input/input.h>

void scene_manager_init(App* app);
bool app_widget_view_input_handler(InputEvent* event, void* context);