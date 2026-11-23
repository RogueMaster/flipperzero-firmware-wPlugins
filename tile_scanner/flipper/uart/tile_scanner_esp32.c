
#include "tile_scanner_esp32.h"

#include <lib/toolbox/bit_buffer.h>

#define LOG_TAG "tile_scanner_esp32"

#define TILE_SCANNER_BAUD_RATE 115200

const uint8_t START_SCANNING_COMMAND[] = {'B'};
const uint8_t STOP_SCANNING_COMMAND[] = {'S'};
const uint8_t TOGGLE_SCANNING_COMMAND[] = {'T'};

static int32_t parse_thread_callback(void* context) {
    TileScanner* scanner = context;
    
    BitBuffer* data_buffer = bit_buffer_alloc(256);
    
    while(scanner->continue_parsing) {
        if(scanner->read_index == scanner->write_index) {
            furi_delay_ms(100);
            continue;
        }
        
        if(scanner->parse_buffer[scanner->read_index] != '\n') {
            bit_buffer_append_byte(data_buffer, scanner->parse_buffer[scanner->read_index]);
            scanner->read_index++;
            continue;
        }
        
        scanner->read_index++;
        
        size_t length = bit_buffer_get_size_bytes(data_buffer);
        uint8_t buf[length + 1];
        bit_buffer_write_bytes(data_buffer, buf, length);
        bit_buffer_reset(data_buffer);
        buf[length] = '\0';
        
        if(length < 2){
            continue;
        }
        
        FURI_LOG_D(LOG_TAG, "Read (%u): %s", length, buf);
        
        // Check for a heartbeat status
        if(buf[0] == 'h') {
            TileScannerHeartbeat heartbeat = {
                .scanning = (buf[1] == '1'),
            };
            
            TileScannerEvent event = {
                .type = TileScannerEventHeartbeat,
                .data = &heartbeat,
            };
            
            if(scanner->callback){
                scanner->callback(event, scanner->callback_context);
            }
            continue;
        }
        
        // Check if it is a mac address
        // Needs to be at least 2 bytes for each byte and spacer (: or ,)
        if(length < 3 * MAC_ADDRESS_LENGTH) {
            FURI_LOG_D(LOG_TAG, "Not enough data for mac address");
            continue;
        }
        
        // Check that the spacers are correct
        if(
            buf[2] != ':' ||
            buf[5] != ':' ||
            buf[8] != ':' ||
            buf[11] != ':' ||
            buf[14] != ':' ||
            buf[17] != ','
        ) {
            FURI_LOG_D(LOG_TAG, "Not the correct spacers");
            continue;
        }
        
        // Note: sscanf does not support parsing hex strings
        TileScannerMacAddress mac_address;
        bool parsed = true;
        
        for(int i = 0; i < MAC_ADDRESS_LENGTH; i++){
            int index = i * 3;
            mac_address.mac_address[i] = 0;
            
            if('0' <= buf[index] && buf[index] <= '9') {
                mac_address.mac_address[i] = buf[index] - '0';
            }else if('a' <= buf[index] && buf[index] <= 'f') {
                mac_address.mac_address[i] = buf[index] - 'a' + 10;
            }else if('A' <= buf[index] && buf[index] <= 'F') {
                mac_address.mac_address[i] = buf[index] - 'A' + 10;
            }else{
                parsed = false;
                break;
            }
            
            mac_address.mac_address[i] = mac_address.mac_address[i] << 4;
            index++;
            
            if('0' <= buf[index] && buf[index] <= '9') {
                mac_address.mac_address[i] |= buf[index] - '0';
            }else if('a' <= buf[index] && buf[index] <= 'f') {
                mac_address.mac_address[i] |= buf[index] - 'a' + 10;
            }else if('A' <= buf[index] && buf[index] <= 'F') {
                mac_address.mac_address[i] |= buf[index] - 'A' + 10;
            }else{
                parsed = false;
                break;
            }
        }
        
        if(parsed) {
            parsed = sscanf((char*)&buf[3 * MAC_ADDRESS_LENGTH], "%ld", &mac_address.rssi);
        }
        
        FURI_LOG_D(
            LOG_TAG,
            "Parsed (%d) items (%02X:%02X:%02X:%02X:%02X:%02X,%ld)",
            parsed,
            mac_address.mac_address[0],
            mac_address.mac_address[1],
            mac_address.mac_address[2],
            mac_address.mac_address[3],
            mac_address.mac_address[4],
            mac_address.mac_address[5],
            mac_address.rssi
        );
        
        if(parsed){
            mac_address.timestamp = furi_hal_rtc_get_timestamp();
            
            TileScannerEvent event = {
                .type = TileScannerEventMacAddress,
                .data = &mac_address,
            };
            
            if(scanner->callback){
                scanner->callback(event, scanner->callback_context);
            }
        }else{
            FURI_LOG_D(LOG_TAG, "Unknown command: %s", buf);
        }
    }
    
    bit_buffer_free(data_buffer);
    
    return 0;
}

static void tile_scanner_rx_callback(FuriHalSerialHandle* uart_handle, FuriHalSerialRxEvent event, void* context) {
    TileScanner* scanner = context;
    
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(uart_handle);
        scanner->parse_buffer[scanner->write_index] = data;
        scanner->write_index++;
    }
}

void tile_scanner_start_scanning(TileScanner* scanner) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scanner);
    
    FURI_LOG_D(LOG_TAG, "Starting Scanner");
    
    furi_hal_serial_tx(scanner->uart_handle, START_SCANNING_COMMAND, sizeof(START_SCANNING_COMMAND));
}

void tile_scanner_stop_scanning(TileScanner* scanner) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scanner);
    
    FURI_LOG_D(LOG_TAG, "Stopping Scanner");
    
    furi_hal_serial_tx(scanner->uart_handle, STOP_SCANNING_COMMAND, sizeof(STOP_SCANNING_COMMAND));
}

void tile_scanner_toggle_scanning(TileScanner* scanner) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scanner);
    
    FURI_LOG_D(LOG_TAG, "Toggling Scanner");
    
    furi_hal_serial_tx(scanner->uart_handle, TOGGLE_SCANNING_COMMAND, sizeof(TOGGLE_SCANNING_COMMAND));
}

TileScanner* tile_scanner_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);
    
    TileScanner* scanner = (TileScanner*)malloc(sizeof(TileScanner));
    scanner->callback = NULL;
    
    scanner->continue_parsing = true;
    scanner->write_index = 0;
    scanner->read_index = 0;
    
    // Spin up a new thread to actual process the RX data since the callback is an ISR
    scanner->parse_thread = furi_thread_alloc();
    furi_thread_set_name(scanner->parse_thread, "TileScannerParseThread");
    furi_thread_set_context(scanner->parse_thread, scanner);
    furi_thread_set_priority(scanner->parse_thread, FuriThreadPriorityHighest);
    furi_thread_set_stack_size(scanner->parse_thread, 2 * 1024);
    furi_thread_set_callback(scanner->parse_thread, parse_thread_callback);
    furi_thread_start(scanner->parse_thread);
    
    scanner->uart_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_assert(scanner->uart_handle, "Failed to acquire serial");
    furi_hal_serial_init(scanner->uart_handle, TILE_SCANNER_BAUD_RATE);
    furi_hal_serial_async_rx_start(scanner->uart_handle, tile_scanner_rx_callback, scanner, true);
    
    return scanner;
}

void tile_scanner_set_callback(TileScanner* scanner, TileScannerCallback callback, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scanner);
    furi_assert(callback);
    
    scanner->callback = callback;
    scanner->callback_context = context;
}

void tile_scanner_free(TileScanner* scanner) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scanner);
    
    scanner->continue_parsing = false;
    furi_thread_join(scanner->parse_thread);
    
    tile_scanner_stop_scanning(scanner);
    furi_hal_serial_tx_wait_complete(scanner->uart_handle);
    
    furi_hal_serial_async_rx_stop(scanner->uart_handle);
    furi_hal_serial_deinit(scanner->uart_handle);
    furi_hal_serial_control_release(scanner->uart_handle);
    
    furi_thread_free(scanner->parse_thread);
    
    free(scanner);
}
