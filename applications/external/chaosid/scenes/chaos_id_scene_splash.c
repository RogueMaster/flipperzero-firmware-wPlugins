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

typedef enum {
    SplashIndexScan,
    SplashIndexHistory,
    SplashIndexAbout,
} SplashIndex;

static void splash_submenu_callback(void* context, uint32_t index) {
    ChaosIdApp* app = context;
    if(index == SplashIndexScan) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventStartScan);
    } else if(index == SplashIndexHistory) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventShowHistory);
    } else if(index == SplashIndexAbout) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventShowAbout);
    }
}

void chaos_id_scene_splash_on_enter(void* context) {
    ChaosIdApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ChaosID");
    submenu_add_item(app->submenu, "Iniciar Scan", SplashIndexScan, splash_submenu_callback, app);
    submenu_add_item(app->submenu, "Historico", SplashIndexHistory, splash_submenu_callback, app);
    submenu_add_item(app->submenu, "Sobre", SplashIndexAbout, splash_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewMenu);
}

bool chaos_id_scene_splash_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ChaosIdCustomEventStartScan:
            scene_manager_next_scene(app->scene_manager, ChaosIdSceneScanning);
            consumed = true;
            break;
        case ChaosIdCustomEventShowHistory:
            scene_manager_next_scene(app->scene_manager, ChaosIdSceneHistory);
            consumed = true;
            break;
        case ChaosIdCustomEventShowAbout:
            scene_manager_next_scene(app->scene_manager, ChaosIdSceneAbout);
            consumed = true;
            break;
        }
    }
    return consumed;
}

void chaos_id_scene_splash_on_exit(void* context) {
    ChaosIdApp* app = context;
    submenu_reset(app->submenu);
}
