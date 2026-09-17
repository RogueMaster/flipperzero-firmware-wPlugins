#pragma once

#include <stdint.h>
#include <gui/view.h>
#include <gui/canvas.h>

#include "fujitsu_general_remote.h"

void view_fujitsu_remote_page2_alloc(FujitsuRemoteApp* app);

void view_fujitsu_remote_page2_add_view(FujitsuRemoteApp* app);

void view_fujitsu_remote_page2_remove_view(FujitsuRemoteApp* app);

void view_fujitsu_remote_page2_free(FujitsuRemoteApp* app);

void view_fujitsu_remote_page2_draw_callback(Canvas* canvas, void* model);

bool view_fujitsu_remote_page2_input_callback(InputEvent* event, void* ctx);

void view_fujitsu_remote_page2_enter_callback(void* ctx);

uint32_t view_fujitsu_remote_page2_exit_callback(void* ctx);
