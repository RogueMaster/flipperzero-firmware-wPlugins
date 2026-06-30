#pragma once

#include "app_types.h"
#include <furi.h>
#include <storage/storage.h>

struct UartStorageContext {
    Storage* storage_api;
    File* current_file;
    File* log_file;
    File* settings_file;
    UartContext* parentContext;
    bool HasOpenedFile;
    bool IsWritingToFile;
    bool view_logs_from_start;
};

UartStorageContext* uart_storage_init(UartContext* parentContext);
void uart_storage_free(UartStorageContext* ctx);
void uart_storage_rx_callback(uint8_t* buf, size_t len, void* context);
