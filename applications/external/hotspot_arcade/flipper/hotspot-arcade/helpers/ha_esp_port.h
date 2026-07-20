#pragma once

#include <furi_hal_serial.h>
#include <stdint.h>

// Flipper "port" for espressif/esp-serial-flasher: the library calls the
// loader_port_* functions (implemented in the .c) to talk to the ESP ROM
// bootloader over our GPIO USART. Attach it to an already-acquired serial handle
// before calling esp_loader_connect_with_stub(), and detach when finished.
//
// The dev board's IO0/EN aren't wired to controllable Flipper GPIO, so the user
// puts the ESP in download mode by hand (hold BOOT, tap RESET, release BOOT) —
// the port's enter_bootloader/reset_target are therefore no-ops.
void ha_esp_port_start(FuriHalSerialHandle* handle, uint32_t baud);
void ha_esp_port_stop(void);

// Drop any buffered RX (between connect attempts while polling for download mode).
void ha_esp_port_flush(void);
