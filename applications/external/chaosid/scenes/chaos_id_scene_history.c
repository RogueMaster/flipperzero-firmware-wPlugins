// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#include "../chaos_id_app.h"
#include "chaos_id_scene.h"
#include "../storage/scan_log.h"

void chaos_id_scene_history_on_enter(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);

    FuriString* content = furi_string_alloc();
    bool has_data = false;
    if(app->storage) {
        has_data = scan_log_read_display(app->storage, content);
    }

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "History");

    if(!has_data || furi_string_size(content) == 0) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "No scans yet");
    } else {
        widget_add_text_scroll_element(app->widget, 0, 14, 128, 50, furi_string_get_cstr(content));
    }

    furi_string_free(content);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewWidget);
}

bool chaos_id_scene_history_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void chaos_id_scene_history_on_exit(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);
}
