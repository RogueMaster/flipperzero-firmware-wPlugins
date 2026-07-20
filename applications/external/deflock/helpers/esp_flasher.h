// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

/*
 * In-app ESP32 flasher: connect (download mode), flash a .bin, and back up the
 * current firmware to SD. Built on Espressif's esp-serial-flasher (Apache-2.0,
 * vendored under lib/esp-serial-flasher). This port + worker is original
 * (GPL-3.0-or-later).
 *
 * Bootloader entry is MANUAL: the user puts the ESP32 into download/bootloader
 * mode (hold BOOT, tap RESET) before connecting. reset/enter-bootloader port
 * hooks are no-ops (board-agnostic).
 */

#include <furi_hal_serial.h>
#include <storage/storage.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct EspFlasher EspFlasher;

/** Log/progress sink. `line` is a NUL-terminated message (no trailing newline). */
typedef void (*EspFlasherLog)(void* ctx, const char* line);

/** Acquire the UART (disables the expansion module). NULL on failure. */
EspFlasher* esp_flasher_alloc(FuriHalSerialId ch, EspFlasherLog log_cb, void* ctx);
void esp_flasher_free(EspFlasher* f);

/**
 * Sync with the target's ROM loader in download mode (NO stub -- like the
 * 0xchocolate ESP Flasher). Retries the SYNC several times. Both flash (write)
 * and backup (read) go through the ROM, so the stub is never uploaded.
 *
 * @param fast_baud non-zero: raise the link to that rate after connecting; on
 *                  failure the connection is aborted, so use Safe (0) instead.
 */
bool esp_flasher_connect(EspFlasher* f, uint32_t fast_baud);

/** Flash `path` to the target at `addr` (use 0 for a merged full image). */
bool esp_flasher_flash_file(EspFlasher* f, Storage* storage, const char* path, uint32_t addr);

/** Dump the whole target flash to `out_path` (full backup). */
bool esp_flasher_backup(EspFlasher* f, Storage* storage, const char* out_path);

/** Request the in-progress flash/backup loop to stop ASAP (e.g. user backs out). */
void esp_flasher_abort(void);
