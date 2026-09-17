#pragma once
#include "../app.h"

// Index constants for the bank-selection VariableItemList
#define CLONE_BANKS_IDX_EPC   0
#define CLONE_BANKS_IDX_TID   1
#define CLONE_BANKS_IDX_USER  2
#define CLONE_BANKS_IDX_RES   3
#define CLONE_BANKS_IDX_START 4

// Navigation callback: Back from bank-selection returns to tag-action menu
uint32_t uhf_reader_navigation_clone_banks_callback(void* context);

// Called when entering the clone-banks view; builds CloneSourceTag deep copy
// and initialises item states based on available bank data.
void view_clone_banks_enter_callback(void* context);

void view_clone_banks_alloc(UHFReaderApp* App);
void view_clone_banks_free(UHFReaderApp* App);
