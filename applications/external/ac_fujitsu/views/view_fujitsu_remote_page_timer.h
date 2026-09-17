#pragma once

#include <stdint.h>
#include <gui/modules/variable_item_list.h>

#include "fujitsu_general_remote.h"

void view_fujitsu_remote_page_timer_alloc(FujitsuRemoteApp* app);

void view_fujitsu_remote_page_timer_add_view(FujitsuRemoteApp* app);

void view_fujitsu_remote_page_timer_remove_view(FujitsuRemoteApp* app);

void view_fujitsu_remote_page_timer_free(FujitsuRemoteApp* app);

uint32_t view_fujitsu_remote_page_timer_exit_callback(void* ctx);
