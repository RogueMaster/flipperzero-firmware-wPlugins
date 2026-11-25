#pragma once

#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>

#define MAC_ADDRESS_LENGTH 6

typedef enum {
    TileScannerEventHeartbeat,
    TileScannerEventMacAddress,
} TileScannerEventType;

typedef struct {
    bool scanning;
} TileScannerHeartbeat;

typedef struct {
    uint8_t mac_address[MAC_ADDRESS_LENGTH];
    uint32_t timestamp;
    int32_t rssi;
} TileScannerMacAddress;

typedef struct {
    TileScannerEventType type;
    void* data;
} TileScannerEvent;

// Called when the NFC instance has got a command
typedef void (*TileScannerCallback)(TileScannerEvent event, void* context);

typedef struct {
    FuriHalSerialHandle* uart_handle;

    FuriThread* parse_thread;
    volatile bool continue_parsing;
    uint8_t parse_buffer[256];
    uint8_t write_index;
    uint8_t read_index;

    TileScannerCallback callback;
    void* callback_context;
} TileScanner;

TileScanner* tile_scanner_alloc();
void tile_scanner_free(TileScanner* scanner);

void tile_scanner_set_callback(TileScanner* scanner, TileScannerCallback callback, void* context);

void tile_scanner_start_scanning(TileScanner* scanner);
void tile_scanner_stop_scanning(TileScanner* scanner);
void tile_scanner_toggle_scanning(TileScanner* scanner);
