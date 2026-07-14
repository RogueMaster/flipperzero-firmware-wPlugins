#ifndef FURI_HAL_SERIAL_H
#define FURI_HAL_SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* Minimal subset of the Flipper Zero Serial HAL for building outside the firmware */

typedef enum {
    FuriHalSerialIdUsart,
    FuriHalSerialIdLpuart,
    FuriHalSerialIdMax,
} FuriHalSerialId;

typedef enum {
    FuriHalBusUSART1,
    FuriHalBusLPUART1,
    FuriHalBusMax,
} FuriHalBus;

typedef struct FuriHalSerialHandle FuriHalSerialHandle;

bool furi_hal_bus_is_enabled(FuriHalBus bus);

FuriHalSerialHandle* furi_hal_serial_control_acquire(FuriHalSerialId serial_id);
void furi_hal_serial_control_release(FuriHalSerialHandle* handle);
void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud);
void furi_hal_serial_deinit(FuriHalSerialHandle* handle);
void furi_hal_serial_tx(FuriHalSerialHandle* handle, const uint8_t* data, size_t length);
void furi_hal_serial_set_br(FuriHalSerialHandle* handle, uint32_t baud);

typedef enum {
    FuriHalSerialRxEventData = (1 << 0),
} FuriHalSerialRxEvent;

typedef void (*FuriHalSerialAsyncRxCallback)(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context);

void furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle);
void furi_hal_serial_async_rx_start(
    FuriHalSerialHandle* handle,
    FuriHalSerialAsyncRxCallback cb,
    void* context,
    bool report_errors);
void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle);
bool furi_hal_serial_async_rx_available(FuriHalSerialHandle* handle);
uint8_t furi_hal_serial_async_rx(FuriHalSerialHandle* handle);

#endif // FURI_HAL_SERIAL_H
