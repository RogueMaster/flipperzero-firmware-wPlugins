#pragma once

#include "app_state.h"

bool storage_scan_directory(SubDupFinderApp *app, const char *dir, ScanStats *stats);
typedef enum { StorageDeleteOk, StorageDeleteChanged, StorageDeleteFailed } StorageDeleteResult;
StorageDeleteResult storage_delete_duplicate(SubDupFinderApp *app, const char *name);
void storage_refresh_duplicates(SubDupFinderApp *app);
bool storage_list_folders(const char *dir, char names[][APP_MAX_PATH_LEN], size_t max_names,
                          size_t *count, size_t *overflow);
