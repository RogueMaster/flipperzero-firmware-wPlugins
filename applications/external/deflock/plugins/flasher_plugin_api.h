// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file flasher_plugin_api.h
 * ABI between the app and the ESP32 flasher plugin. Included by BOTH sides.
 *
 * WHY THE FLASHER IS NOT IN THE APP -- the Flipper's loader has to place the
 * .fap in one contiguous allocation, and users on heavier firmware were being
 * refused with "Not enough RAM to run the app" (issue #5). The flasher, with
 * Espressif's vendored esp-serial-flasher underneath it, is reachable from
 * exactly one screen (Firmware), so it is loaded on demand and dropped again on
 * the way out.
 *
 * MEASURED, before and after, with arm-none-eabi-size on the linked .elf:
 *
 *     .text    58,060 -> 49,986   -8,074
 *     .rodata  16,224 -> 14,186   -2,038
 *     .bss      1,313 ->  1,196     -117
 *     total    75,598 -> 65,368  -10,230   (13.5% smaller)
 *
 * Take the measured number, not the obvious one. Summing the .o sections says
 * the flasher is ~27.6 KB, and that is wrong as a statement about the image:
 * ~13.4 KB of it is esp_stubs.c, a blob the linker discards because this port
 * never uploads a stub (see esp_flasher.c -- both flash and backup go through
 * the ROM loader). Object size is not image size when --gc-sections is on.
 *
 * For scale, the candidates considered instead: Net Guardian ~3.8 KB and the
 * Wi-Fi audit ~2.5 KB by the same .o-sum measure, so subject to the same
 * over-count and smaller again once linked.
 *
 * The .fal ships INSIDE the .fap as a file asset (fal_embedded), so this stays a
 * single-file install.
 *
 * LIFETIME RULE, and it is the one that bites: the flash/backup runs on a WORKER
 * THREAD executing code that lives inside the mapped .fal. The plugin must stay
 * loaded until that thread has been joined. Unmapping first unmaps running code.
 * See recon_scene_firmware_run.c, which aborts, joins, and only then frees.
 *
 * VERSIONING: bump FLASHER_PLUGIN_API_VERSION on any change to
 * FlasherPluginApi. The loader refuses a mismatched plugin rather than calling
 * through a stale layout, and the app must treat a refusal as "feature
 * unavailable", never as a crash.
 */
#pragma once

#include <furi_hal_serial.h>
#include <storage/storage.h>
#include <stdbool.h>
#include <stdint.h>

#define FLASHER_PLUGIN_APP_ID      "flipdeflock_flasher"
#define FLASHER_PLUGIN_API_VERSION 1

/** Opaque to the app: it only ever holds and passes back the pointer. */
typedef struct EspFlasher EspFlasher;

/** Log/progress sink. `line` is a NUL-terminated message (no trailing newline). */
typedef void (*EspFlasherLog)(void* ctx, const char* line);

typedef struct {
    /** Acquire the UART (disables the expansion module). NULL on failure. */
    EspFlasher* (*alloc)(FuriHalSerialId ch, EspFlasherLog log_cb, void* ctx);

    /** Release the UART and free. NULL-safe. */
    void (*free)(EspFlasher* f);

    /**
     * Sync with the target's ROM loader in download mode (NO stub uploaded).
     * `fast_baud` non-zero raises the link after connecting; on failure the
     * connection is aborted, so use 0 (Safe) instead.
     */
    bool (*connect)(EspFlasher* f, uint32_t fast_baud);

    /** Flash `path` to the target at `addr` (0 for a merged full image). */
    bool (*flash_file)(EspFlasher* f, Storage* storage, const char* path, uint32_t addr);

    /** Dump the whole target flash to `out_path`. */
    bool (*backup)(EspFlasher* f, Storage* storage, const char* out_path);

    /** Ask an in-progress flash/backup to stop ASAP (user backed out). */
    void (*abort)(void);
} FlasherPluginApi;
