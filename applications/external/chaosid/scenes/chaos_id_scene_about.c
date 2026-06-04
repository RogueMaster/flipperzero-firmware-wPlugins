// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana
//
// This file is part of ChaosID.
// ChaosID is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version. See <https://www.gnu.org/licenses/gpl-3.0.html>.

#include "../chaos_id_app.h"
#include "chaos_id_scene.h"

void chaos_id_scene_about_on_enter(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "ChaosID");
    widget_add_text_scroll_element(
        app->widget,
        0,
        14,
        128,
        50,
        "Perito forense de cartoes\n"
        "LF (125 kHz) + HF (13.56 MHz)\n"
        "\n"
        "Identifica protocolo, expoe\n"
        "vetor de ataque e nivel de\n"
        "risco do cartao detectado.\n"
        "\n"
        "by Cafe com Solda\n"
        "Chaos IAM series\n"
        "v0.1 - POC");
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewWidget);
}

bool chaos_id_scene_about_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void chaos_id_scene_about_on_exit(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);
}
