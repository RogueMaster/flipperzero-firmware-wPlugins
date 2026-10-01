#pragma once

#include "app_state.h"

bool storage_scan_directory(SubDupFinderApp *app, const char *dir, ScanStats *stats);
bool storage_delete_file(const char *path);
bool storage_list_folders(const char *dir, char names[][APP_MAX_PATH_LEN], size_t max_names,
                          size_t *count, size_t *overflow);
