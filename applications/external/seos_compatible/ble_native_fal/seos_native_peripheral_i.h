#pragma once

#include "seos_i.h"
#include "seos_native_peripheral.h"

/* The worker's own stop flag. It used to borrow the one the external BLE
 * path's UART worker declares, which is the only reason this file saw that
 * header at all. */
typedef enum {
    NativePeripheralEvtStop = (1 << 0),
} NativePeripheralEvtFlags;
