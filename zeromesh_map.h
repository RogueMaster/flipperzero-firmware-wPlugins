#pragma once

#include "zeromesh_serial.h"

#define MAP_W       128
#define MAP_H       64
#define MAP_MIN_Z   1
#define MAP_MAX_Z   14

bool map_alloc(ZeroMeshApp* app);

void map_free(ZeroMeshApp* app);

void map_tick(ZeroMeshApp* app);

void render_map(Canvas* canvas, ZeroMeshApp* app);

bool map_wants_key(ZeroMeshApp* app, InputKey key);
void input_map(InputEvent* e, ZeroMeshApp* app);
