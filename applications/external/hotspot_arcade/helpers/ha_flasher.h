#pragma once

#include <furi.h>
#include "../ha_uart.h"

// Progress callback, invoked from the flashing worker thread. `stage` is a short
// label (the image filename). pct is 0..100 for the current image. It must only
// signal the GUI (post an event), not block.
typedef void (*HaFlashProgress)(
    void* ctx,
    uint8_t img_index,
    uint8_t img_count,
    uint8_t pct,
    const char* stage);

// Read the manifest (a "flash.txt" of "<offset> <filename>" lines, filenames
// relative to the manifest's folder), borrow the serial line from `uart`, then
// POLL for the ESP in download mode (retrying the stub-loader connect) until it
// answers or `*cancel` goes true. On connect it calls `on_connected(cb_ctx)` (so
// the caller can lock the UI), then flashes each image with MD5 verify.
// Blocking — call from a worker thread. Returns true on success.
// With `auto_boot` the DTR/RTS lines are pulsed once up front to drop the board
// into download mode; otherwise the user does the BOOT/RESET dance by hand.
bool ha_flasher_run(
    HaUart* uart,
    const char* manifest_path,
    bool auto_boot,
    HaFlashProgress cb,
    void* cb_ctx,
    volatile bool* cancel,
    void (*on_connected)(void* ctx),
    char* err,
    size_t err_size);
