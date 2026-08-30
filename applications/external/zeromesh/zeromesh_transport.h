#pragma once

#include "zeromesh_serial.h"

void transport_open(ZeroMeshApp* app);

void transport_close(ZeroMeshApp* app);

void transport_set(ZeroMeshApp* app, ZmTransport t);

bool transport_is_up(ZeroMeshApp* app);

void transport_tx(ZeroMeshApp* app, const uint8_t* data, size_t len);

const char* transport_name(ZeroMeshApp* app);
