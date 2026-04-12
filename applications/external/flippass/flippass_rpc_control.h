#pragma once

#include "flippass.h"

bool flippass_rpc_init(App* app, const char* args);
void flippass_rpc_deinit(App* app);
bool flippass_rpc_handle_custom_event(App* app, uint32_t event);
