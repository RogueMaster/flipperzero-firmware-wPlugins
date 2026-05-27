#pragma once

#include "zeromesh_serial.h"

void history_add(ZeroMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx);
void log_line(ZeroMeshApp* app, const char* fmt, ...);
void set_status(ZeroMeshApp* app, const char* fmt, ...);