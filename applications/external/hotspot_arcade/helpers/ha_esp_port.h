#pragma once

#include <furi_hal_serial.h>
#include <stdint.h>

// Flipper "port" for espressif/esp-serial-flasher: the library calls the
// loader_port_* functions (implemented in the .c) to talk to the ESP ROM
// bootloader over our GPIO USART. Attach it to an already-acquired serial handle
// before calling esp_loader_connect_with_stub(), and detach when finished.
//
// Boards whose EN/IO0 reach the Flipper's DTR/RTS pins can be dropped into
// download mode from here (ha_esp_port_enter_bootloader); on the rest the user
// still does it by hand (hold BOOT, tap RESET, release BOOT).
void ha_esp_port_start(FuriHalSerialHandle* handle, uint32_t baud);
void ha_esp_port_stop(void);

// Drop any buffered RX (between connect attempts while polling for download mode).
void ha_esp_port_flush(void);

// Point the port at the worker's cancel flag, so a Back press unblocks it.
// The loader library only ever blocks inside loader_port_read/delay_ms, and its
// timeouts run from 100 ms up to tens of seconds; without this the scene's
// on_exit join can sit there for a long time with the UI frozen. NULL detaches.
void ha_esp_port_set_cancel(volatile bool* cancel);

// Pulse DTR/RTS (and OTG power) to put the board in the ROM bootloader, so the
// user doesn't have to do the BOOT/RESET dance themselves.
void ha_esp_port_enter_bootloader(void);

// Pulse DTR alone to reboot the board into the firmware just written. Used on
// the ROM path, whose FLASH_END-with-reboot the C5 refuses. A no-op in practice
// on boards whose reset line isn't wired to these pins — the done screen still
// tells the user to tap RESET.
void ha_esp_port_reset_target(void);
