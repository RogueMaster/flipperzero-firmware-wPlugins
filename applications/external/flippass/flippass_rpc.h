#pragma once

#include <stdbool.h>

typedef struct App App;

bool flippass_rpc_init(App* app, const char* args);
void flippass_rpc_deinit(App* app);
