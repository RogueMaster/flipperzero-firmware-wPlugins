#pragma once
#include "../app.h"

#define UHF_SAVED_PAGE_SIZE           8U
#define UHF_SAVED_DELETE_ALL_INDEX    0xFFFFFFFDU
#define UHF_SAVED_PREVIOUS_PAGE_INDEX 0xFFFFFFFEU
#define UHF_SAVED_NEXT_PAGE_INDEX     0xFFFFFFFFU

//Function Declarations
uint32_t uhf_reader_navigation_saved_exit_callback(void* context);

void uhf_reader_submenu_saved_callback(void* context, uint32_t index);
void uhf_reader_saved_menu_rebuild(UHFReaderApp* App);

void view_saved_menu_alloc(UHFReaderApp* App);

void view_saved_free(UHFReaderApp* App);
