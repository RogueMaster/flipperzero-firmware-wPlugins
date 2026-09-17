#pragma once
#include "../app.h"

void uhf_reader_show_antenna_info(UHFReaderApp* App, bool startup);
void uhf_reader_show_antenna_connecting(UHFReaderApp* App, uint8_t attempt, uint8_t total);
void uhf_reader_show_antenna_not_connected(UHFReaderApp* App, uint32_t return_view);
void view_antenna_info_alloc(UHFReaderApp* App);
void view_antenna_info_free(UHFReaderApp* App);
