#pragma once
#include "../app.h"

void uhf_reader_view_umi_test_draw_callback(Canvas* canvas, void* model);
bool uhf_reader_view_umi_test_input_callback(InputEvent* event, void* context);
bool uhf_reader_view_umi_test_custom_event_callback(uint32_t event, void* context);
void uhf_reader_view_umi_test_enter_callback(void* context);
void uhf_reader_view_umi_test_exit_callback(void* context);
uint32_t uhf_reader_navigation_umi_test_callback(void* context);

void uhf_umi_test_worker_callback(UHFWorkerEvent event, void* context);

void view_umi_test_alloc(UHFReaderApp* App);
void view_umi_test_free(UHFReaderApp* App);
