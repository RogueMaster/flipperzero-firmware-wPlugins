
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/button_menu.h>

#define FRAME_WIDTH  128
#define FRAME_HEIGHT 64

typedef struct LaserTagApp LaserTagApp;

LaserTagApp* laser_tag_app_alloc();
void laser_tag_app_free(LaserTagApp* app);
int32_t laser_tag_app(void* p);
void laser_tag_app_set_view_port(LaserTagApp* app, View* view);
void laser_tag_app_switch_to_next_scene(LaserTagApp* app);
void laser_tag_app_fire(LaserTagApp* app);
void laser_tag_app_handle_hit(LaserTagApp* app);
