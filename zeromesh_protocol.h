#pragma once

#include "zeromesh_serial.h"

void send_text_message(ZeroMeshApp* app, const char* text, uint32_t to_node);
void request_info(ZeroMeshApp* app);
void request_position(ZeroMeshApp* app, uint32_t to_node);
void set_node_lora(ZeroMeshApp* app, uint8_t region, uint8_t preset);
void set_node_role(ZeroMeshApp* app, uint8_t role);
void set_node_owner(ZeroMeshApp* app, const char* long_name, const char* short_name);
void reboot_node(ZeroMeshApp* app, int32_t seconds);
void request_node_info(ZeroMeshApp* app, uint32_t to_node);
void send_heartbeat(ZeroMeshApp* app);
int32_t rx_thread_fn(void* ctx);