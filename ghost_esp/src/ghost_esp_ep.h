#pragma once

#include "app_state.h"

bool ghost_esp_ep_read_html_file(AppState* app, uint8_t** the_html, size_t* html_size);
bool ghost_esp_ep_read_ir_file(AppState* app, uint8_t** ir_data, size_t* ir_size);
