#pragma once
#include "../app.h"

// Per-bank memory screen: shows TID / Reserved / User one bank at a time,
// reading each bank on demand (OK), cycling banks with More (Right), and
// vertically scrolling long content (Up/Down). Replaces the old all-at-once
// Banks screen for the live EPC-dump flow.

void uhf_reader_view_bank_mem_draw_callback(Canvas* canvas, void* model);
bool uhf_reader_view_bank_mem_input_callback(InputEvent* event, void* context);
bool uhf_reader_view_bank_mem_custom_event_callback(uint32_t event, void* context);
void uhf_reader_view_bank_mem_enter_callback(void* context);
uint32_t uhf_reader_navigation_bank_mem_to_epc_dump_callback(void* context);
void uhf_single_bank_read_worker_callback(UHFWorkerEvent event, void* ctx);
void view_bank_mem_alloc(UHFReaderApp* App);
void view_bank_mem_free(UHFReaderApp* App);
