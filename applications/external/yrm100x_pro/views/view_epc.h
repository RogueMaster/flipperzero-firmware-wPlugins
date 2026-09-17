#pragma once
#include "../app.h"

//Function Declarations
void uhf_reader_view_epc_draw_callback(Canvas* canvas, void* model);
bool uhf_reader_view_epc_input_callback(InputEvent* event, void* context);
bool uhf_reader_view_epc_custom_event_callback(uint32_t event, void* context);
uint32_t uhf_reader_navigation_exit_view_epc_callback(void* context);
uint32_t uhf_reader_navigation_banks_to_epc_dump_callback(void* context);
void uhf_reader_view_epc_enter_callback(void* context);
void uhf_reader_view_epc_exit_callback(void* context);
void view_epc_alloc(UHFReaderApp* App);
void view_epc_free(UHFReaderApp* App);
