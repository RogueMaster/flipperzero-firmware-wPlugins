// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#pragma once

#include <gui/scene_manager.h>

void chaos_id_scene_splash_on_enter(void* context);
bool chaos_id_scene_splash_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_splash_on_exit(void* context);

void chaos_id_scene_scanning_on_enter(void* context);
bool chaos_id_scene_scanning_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_scanning_on_exit(void* context);

void chaos_id_scene_result_on_enter(void* context);
bool chaos_id_scene_result_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_result_on_exit(void* context);

void chaos_id_scene_history_on_enter(void* context);
bool chaos_id_scene_history_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_history_on_exit(void* context);

void chaos_id_scene_about_on_enter(void* context);
bool chaos_id_scene_about_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_about_on_exit(void* context);

void chaos_id_scene_attack_on_enter(void* context);
bool chaos_id_scene_attack_on_event(void* context, SceneManagerEvent event);
void chaos_id_scene_attack_on_exit(void* context);

extern const SceneManagerHandlers chaos_id_scene_handlers;
