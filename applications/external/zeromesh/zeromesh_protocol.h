#pragma once

#include "zeromesh_serial.h"

void send_text_message(ZeroMeshApp* app, const char* text, uint32_t to_node);
void request_info(ZeroMeshApp* app);
void request_position(ZeroMeshApp* app, uint32_t to_node);
void set_node_lora(ZeroMeshApp* app, uint8_t region, uint8_t preset);
void set_node_role(ZeroMeshApp* app, uint8_t role);
void set_node_gps(ZeroMeshApp* app, bool enabled);
void request_channel(ZeroMeshApp* app, uint8_t index);
void request_position_config(ZeroMeshApp* app);
void set_fixed_position(ZeroMeshApp* app, int32_t lat_i, int32_t lon_i);
void clear_fixed_position(ZeroMeshApp* app);
void set_channel_config(ZeroMeshApp* app, bool make_private, bool share_position);
void set_node_owner(ZeroMeshApp* app, const char* long_name, const char* short_name);
void reboot_node(ZeroMeshApp* app, int32_t seconds);
void request_node_info(ZeroMeshApp* app, uint32_t to_node);
void send_heartbeat(ZeroMeshApp* app);
int32_t rx_thread_fn(void* ctx);
