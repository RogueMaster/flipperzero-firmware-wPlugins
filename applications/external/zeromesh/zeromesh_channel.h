#pragma once

#include "zeromesh_serial.h"

void channel_init(ZeroMeshApp* app);
void channel_next(ZeroMeshApp* app);
void channel_set(ZeroMeshApp* app, uint8_t channel);
const char* channel_get_name(uint8_t channel);