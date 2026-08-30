#pragma once

#include "zeromesh_serial.h"

#define MAP_W     128
#define MAP_H     64
#define MAP_MIN_Z 1
#define MAP_MAX_Z 14

bool map_alloc(ZeroMeshApp* app);

void map_free(ZeroMeshApp* app);

void map_tick(ZeroMeshApp* app);

bool map_focus_node(ZeroMeshApp* app, uint32_t node_id);

bool map_view_center(ZeroMeshApp* app, int32_t* lat_i, int32_t* lon_i);

void render_map(Canvas* canvas, ZeroMeshApp* app);

bool map_wants_key(ZeroMeshApp* app, InputKey key);
void input_map(InputEvent* e, ZeroMeshApp* app);
