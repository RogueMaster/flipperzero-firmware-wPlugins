#pragma once
#include "../app.h"
#include "../helpers/yrm100x_module.h"
#include "../helpers/yrm100x_tag.h"

// Draw callback for the clone scan/confirm/write view
void uhf_reader_view_clone_draw_callback(Canvas* canvas, void* model);

// Input callback: OK on the confirm screen triggers the write
bool uhf_reader_view_clone_input_callback(InputEvent* event, void* context);

// Custom-event callback: handles worker results forwarded via view_dispatcher
bool uhf_reader_view_clone_custom_event_callback(uint32_t event, void* context);

// View lifecycle
void uhf_reader_view_clone_enter_callback(void* context);
void uhf_reader_view_clone_exit_callback(void* context);

// Worker callback (fires from the UHFWorker thread, sends custom events)
void uhf_clone_worker_callback(UHFWorkerEvent event, void* context);

// Navigation: Back from clone scan screen returns to bank-selection
uint32_t uhf_reader_navigation_clone_callback(void* context);

void view_clone_alloc(UHFReaderApp* App);
void view_clone_free(UHFReaderApp* App);
