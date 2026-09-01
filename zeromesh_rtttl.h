#pragma once

#include "zeromesh_serial.h"

void ringtones_scan(ZeroMeshApp* app);
uint16_t ringtone_total(const ZeroMeshApp* app);
void ringtone_label(const ZeroMeshApp* app, uint16_t index, char* out, size_t cap);
int16_t ringtone_index_of(const ZeroMeshApp* app, const char* filename);
bool rtttl_play_custom(const ZeroMeshApp* app, uint16_t index);
