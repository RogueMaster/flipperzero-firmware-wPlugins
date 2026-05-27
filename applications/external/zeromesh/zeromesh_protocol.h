#pragma once

#include "zeromesh_serial.h"

void send_text_message(ZeroMeshApp* app, const char* text, uint32_t to_node);
void request_info(ZeroMeshApp* app);
int32_t rx_thread_fn(void* ctx);