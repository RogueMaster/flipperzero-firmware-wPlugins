/* Control over the FFI platform implementation, used by ffi/dfc_ffi.c. */
#pragma once

#include "dfc_ffi.h"

// Install the random source the engine draws from on this thread, or NULL for
// the operating system's generator.
void dfc_ffi_port_set_random(DfcFfiRandomCallback random, void* context);
void dfc_ffi_port_system_random(uint8_t* buffer, size_t len);
