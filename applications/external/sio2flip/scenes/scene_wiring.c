/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/app.h"

#include "scenes.h"

void scene_wiring_on_enter(void* context) {
    App* app = (App*)context;

    popup_set_header(app->popup, "Wiring", 0, 0, AlignLeft, AlignTop);

    popup_set_text(
        app->popup,
        "TX (13) - DIN\n"
        "RX (14) - DOUT\n"
        "C0 (16) - COMMAND\n"
        "GND (18) - GND\n",
        4,
        16,
        AlignLeft,
        AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPopup);
}

bool scene_wiring_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_wiring_on_exit(void* context) {
    App* app = (App*)context;

    popup_reset(app->popup);
}
