/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef LOAD_SCENE_H
#define LOAD_SCENE_H

#include <furi.h>
#include <gui/scene_manager.h>

typedef enum {
    UDECardLoadSceneSuccessEvent,
    UDECardLoadSceneFailedEvent,
    UDECardLoadSceneCancelledEvent
} UDECardLoadSceneEvent;

void udecard_load_scene_on_enter(void* context);
bool udecard_load_scene_on_event(void* context, SceneManagerEvent event);
void udecard_load_scene_on_exit(void* context);

#endif
