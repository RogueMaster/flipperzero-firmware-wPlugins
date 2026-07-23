// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "scene_util.h"
#include "../recon_app_i.h"

void scene_show_companion_guard(void* _app, const char* message) {
    ReconApp* app = _app;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, message);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

void scene_report_notify(void* _app, bool ok) {
    ReconApp* app = _app;
    if(app->settings.sound) {
        notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    }
}
