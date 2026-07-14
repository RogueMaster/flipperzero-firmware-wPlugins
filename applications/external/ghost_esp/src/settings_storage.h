#pragma once
#include "settings_def.h"
#include <furi.h>
#include <storage/storage.h>
#include <furi_hal.h>

// Forward declare the struct to avoid circular dependencies
struct UartStorageContext;

// Initialize settings storage
bool settings_storage_init();
void uart_storage_sync_file(struct UartStorageContext* ctx);
__attribute__((used)) SettingsResult settings_storage_save(Settings* settings, const char* path);
__attribute__((used)) SettingsResult settings_storage_load(Settings* settings, const char* path);
