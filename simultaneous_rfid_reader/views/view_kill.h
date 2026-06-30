#pragma once
#include "../app.h"

//Function Declarations
void uhf_reader_submenu_kill_callback(void* context, uint32_t index);

uint32_t uhf_reader_navigation_kill_callback(void* context);

uint32_t uhf_reader_navigation_kill_exit_callback(void* context);

void view_kill_alloc(UHFReaderApp* App);

void view_kill_free(UHFReaderApp* App);

void uhf_reader_fetch_selected_tag(void* context);