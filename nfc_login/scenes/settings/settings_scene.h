#pragma once

#include "nfc_login_app.h"

void app_render_settings(App* app);
void app_render_credits(App* app);
void app_render_lockscreen(App* app);
void app_render_passcode_dots(App* app, size_t max_buttons);