#pragma once

#include "zeromesh_serial.h"

void render_nodecfg(Canvas* canvas, ZeroMeshApp* app);
void input_nodecfg(InputEvent* e, ZeroMeshApp* app);
bool nodecfg_wants_key(ZeroMeshApp* app, InputKey key);
