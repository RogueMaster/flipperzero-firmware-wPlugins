#pragma once

#include "zeromesh_serial.h"

void roster_add_node(ZeroMeshApp* app, uint32_t node_id, int8_t snr, int16_t rssi);
void roster_update_telemetry(ZeroMeshApp* app, uint32_t node_id, uint8_t battery_level, float voltage);
void render_roster(Canvas* canvas, ZeroMeshApp* app);
void input_roster(InputEvent* e, ZeroMeshApp* app);