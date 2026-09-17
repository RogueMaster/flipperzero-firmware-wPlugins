#pragma once
#include "../app.h"

void uhf_reader_submenu_lock_callback(void* context, uint32_t index);

uint32_t uhf_reader_navigation_lock_callback(void* context);

uint32_t uhf_reader_navigation_lock_exit_callback(void* context);

void view_lock_alloc(UHFReaderApp* App);

void view_lock_free(UHFReaderApp* App);

void uhf_reader_current_ap_updated(void* context);
void uhf_reader_new_ap_updated(void* context);
void uhf_reader_current_kp_updated(void* context);
void uhf_reader_new_kp_updated(void* context);
