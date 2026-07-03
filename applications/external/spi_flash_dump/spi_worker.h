#pragma once

#include "spi_flash_dump.h"

/* ------------------------------------------------------------------ */
/*  SPI Worker – bit-banged SPI over external GPIO                    */
/* ------------------------------------------------------------------ */

/**
 * Allocate and return a new SpiWorker.
 * Does NOT initialise GPIO – call spi_worker_gpio_init() explicitly.
 */
SpiWorker* spi_worker_alloc(void);

/**
 * Free all resources owned by the worker (thread, etc.).
 * Does NOT deinitialise GPIO – call spi_worker_gpio_deinit() first.
 */
void spi_worker_free(SpiWorker* worker);

/* ---- GPIO lifecycle ---- */
void spi_worker_gpio_init(void);
void spi_worker_gpio_deinit(void);

/* ---- Low-level SPI helpers (exposed for direct use) ---- */

/** Pull CS low (active). */
void spi_cs_low(void);

/** Release CS high (inactive). */
void spi_cs_high(void);

/** Bit-bang one byte over SPI (simultaneous TX/RX, MSB-first, Mode 0). */
uint8_t spi_transfer_byte(uint8_t tx, uint32_t delay_us);

/* ---- Chip-level operations ---- */

/**
 * Send JEDEC Read-ID (0x9F) and look up the result in the built-in
 * database.  Returns true if a known chip was found.
 * `info_out` is always populated with the raw IDs even when not found.
 */
bool chip_detect(SpiFlashChipInfo* info_out, uint32_t delay_us);

/**
 * Read the full contents of the chip into the file at `path`.
 * Calls `cb` after every page so the UI can update.
 * Returns true on success.
 */
bool chip_read(
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay_us,
    SpiProgressCallback cb,
    void* cb_ctx);

/**
 * Re-read the chip and compare against the file at `path`.
 * `match_out` / `mismatch_out` count individual bytes.
 * Returns true if every byte matched.
 */
bool chip_verify(
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay_us,
    SpiProgressCallback cb,
    void* cb_ctx,
    uint32_t* match_out,
    uint32_t* mismatch_out);

/** Read Status Register 1 (cmd 0x05). */
uint8_t read_status_register(uint32_t delay_us);

/** Read Status Register 2 (cmd 0x35). */
uint8_t read_status_register2(uint32_t delay_us);

/** Send Release Power-Down / Device ID (0xAB). */
void chip_release_power_down(uint32_t delay_us);

/* ---- Threaded worker (non-blocking read/verify) ---- */

/**
 * Start a background read operation.
 * The worker thread will call `cb` with progress updates.
 */
void spi_worker_start_read(
    SpiWorker* worker,
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay_us,
    SpiProgressCallback cb,
    void* cb_ctx);

/**
 * Start a background verify operation.
 */
void spi_worker_start_verify(
    SpiWorker* worker,
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay_us,
    SpiProgressCallback cb,
    void* cb_ctx,
    uint32_t* match_out,
    uint32_t* mismatch_out);

/** Returns true while the worker thread is still running. */
bool spi_worker_is_running(SpiWorker* worker);

/** Block until the worker thread finishes. */
void spi_worker_wait(SpiWorker* worker);

/** Get the result of the last operation (true = success). */
bool spi_worker_get_result(SpiWorker* worker);

/** Get CRC32 of the dump file (only valid after a successful read). */
uint32_t spi_worker_get_crc32(SpiWorker* worker);

/** Returns true if CRC32 was computed (read succeeded and file was re-read). */
bool spi_worker_has_crc32(SpiWorker* worker);
