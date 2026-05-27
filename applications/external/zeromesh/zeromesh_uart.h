#pragma once

#include "zeromesh_serial.h"

void uart_close(ZeroMeshApp* app);
void uart_open(ZeroMeshApp* app);
void uart_reopen(ZeroMeshApp* app, FuriHalSerialId new_id, uint32_t new_baud);