#pragma once

#include <gui/scene_manager.h>

void updi_scene_menu_on_enter(void* context);
bool updi_scene_menu_on_event(void* context, SceneManagerEvent event);
void updi_scene_menu_on_exit(void* context);

void updi_scene_pins_on_enter(void* context);
bool updi_scene_pins_on_event(void* context, SceneManagerEvent event);
void updi_scene_pins_on_exit(void* context);

void updi_scene_wiring_on_enter(void* context);
bool updi_scene_wiring_on_event(void* context, SceneManagerEvent event);
void updi_scene_wiring_on_exit(void* context);

void updi_scene_about_on_enter(void* context);
bool updi_scene_about_on_event(void* context, SceneManagerEvent event);
void updi_scene_about_on_exit(void* context);

void updi_scene_detect_on_enter(void* context);
bool updi_scene_detect_on_event(void* context, SceneManagerEvent event);
void updi_scene_detect_on_exit(void* context);

void updi_scene_dump_name_on_enter(void* context);
bool updi_scene_dump_name_on_event(void* context, SceneManagerEvent event);
void updi_scene_dump_name_on_exit(void* context);

void updi_scene_fuse_write_on_enter(void* context);
bool updi_scene_fuse_write_on_event(void* context, SceneManagerEvent event);
void updi_scene_fuse_write_on_exit(void* context);

void updi_scene_work_on_enter(void* context);
bool updi_scene_work_on_event(void* context, SceneManagerEvent event);
void updi_scene_work_on_exit(void* context);

extern const SceneManagerHandlers updi_scene_handlers;
