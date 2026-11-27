#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>

typedef struct BarCodeScript BarCodeScript;

typedef enum {
    BarCodeStateInit,
    BarCodeStateIdle,
    BarCodeStateRunning,
    BarCodeStateDelay,
    BarCodeStateDone,
    BarCodeStateScriptError,
    BarCodeStateFileError,
} BarCodeWorkerState;

typedef enum {
    FileIsEmpty,
    FileOpenError
} FileError;

typedef struct {
    BarCodeWorkerState state;
    uint16_t line_cur;
    uint16_t line_nb;
    uint32_t delay_remain;
    int error_enum;
    char error[64];
} BarCodeState;

BarCodeScript* bc_scanner_script_open(FuriString* file_path);
void bc_scanner_script_close(BarCodeScript* bc_script);

/**
 * Open the script object in RPC mode: USB CDC is initialised but no worker thread or file is
 * used. Received bytes are relayed to the USB CDC by bc_scanner_script_send_data().
 */
BarCodeScript* bc_scanner_script_open_rpc(void);

/**
 * Relay @p size bytes from @p data to the USB CDC as-is (no terminator, research R10).
 *
 * @return true when the bytes were handed to the USB CDC, false on invalid input.
 */
bool bc_scanner_script_send_data(BarCodeScript* bc_script, const uint8_t* data, size_t size);

void bc_scanner_script_toggle(BarCodeScript* bc_script);

BarCodeState* bc_scanner_script_get_state(BarCodeScript* bc_script);

#ifdef __cplusplus
}
#endif
