#pragma once

#include "zeromesh_serial.h"

void roster_add_node(ZeroMeshApp* app, uint32_t node_id, int8_t snr, int16_t rssi);
void roster_update_telemetry(
    ZeroMeshApp* app,
    uint32_t node_id,
    uint8_t battery_level,
    float voltage);
void roster_update_name(
    ZeroMeshApp* app,
    uint32_t node_id,
    const char* short_name,
    const char* long_name);
void roster_update_position(
    ZeroMeshApp* app,
    uint32_t node_id,
    int32_t latitude_i,
    int32_t longitude_i,
    int32_t altitude,
    uint32_t pos_time);
void roster_update_sats(ZeroMeshApp* app, uint32_t node_id, uint8_t sats, bool fix);
void roster_update_fix(ZeroMeshApp* app, uint32_t node_id, bool fix);
void render_roster(Canvas* canvas, ZeroMeshApp* app);
void input_roster(InputEvent* e, ZeroMeshApp* app);
