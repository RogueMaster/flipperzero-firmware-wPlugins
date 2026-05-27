#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define STORAGE_PATH "/ext/apps_data/nfc_stock_manager"
#define CONFIG_FILE "/ext/apps_data/nfc_stock_manager/active_db.txt"
#define DEFAULT_DB_PATH "/ext/apps_data/nfc_stock_manager/default.bin"

bool storage_helper_get_active_db(char *path, size_t size);

bool storage_helper_set_active_db(const char *path);

void storage_helper_ensure_default(void);
