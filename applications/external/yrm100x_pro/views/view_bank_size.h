#pragma once
#include "../app.h"

void uhf_reader_view_bank_size_draw_callback(Canvas* canvas, void* model);
bool uhf_reader_view_bank_size_input_callback(InputEvent* event, void* context);
bool uhf_reader_view_bank_size_custom_event_callback(uint32_t event, void* context);
void uhf_reader_view_bank_size_enter_callback(void* context);
void uhf_reader_view_bank_size_exit_callback(void* context);
uint32_t uhf_reader_navigation_bank_size_callback(void* context);
void uhf_bank_size_worker_callback(UHFWorkerEvent event, void* context);
void view_bank_size_alloc(UHFReaderApp* App);
void view_bank_size_free(UHFReaderApp* App);
