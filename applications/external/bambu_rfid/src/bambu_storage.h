#pragma once

#include "bambu_rfid.h"

void br_storage_init(Storage* storage);
uint16_t br_saved_scan(Storage* storage, BrSavedEntry* entries, uint16_t max_entries);
bool br_saved_path(const char* filename, char* out, size_t out_size);
