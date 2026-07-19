#pragma once
#include "furi_hal.h"
#include "furi_hal_serial_types.h"

FuriHalSerialHandle* furi_hal_serial_control_acquire(FuriHalSerialId id);
void furi_hal_serial_control_release(FuriHalSerialHandle* h);
void furi_hal_serial_configure_framing(
    FuriHalSerialHandle* h,
    FuriHalSerialDataBits d,
    FuriHalSerialParity p,
    FuriHalSerialStopBits s);
