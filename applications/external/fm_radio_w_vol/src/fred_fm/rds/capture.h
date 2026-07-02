/**
 * @file capture.h
 * @brief RDS raw ADC capture to SD (debug mode, writer thread).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>
#include "src/fred_fm/include/config.h"
#include "src/drivers/rds/RDSAcquisition.h"

extern volatile bool rds_capture_active;
extern volatile bool rds_capture_requested;
extern volatile bool rds_capture_finalize_pending;
extern volatile bool rds_capture_abort_pending;
extern volatile bool rds_capture_error;
extern volatile bool rds_capture_complete;
extern uint32_t rds_capture_captured_blocks;
extern uint32_t rds_capture_written_blocks;
extern uint32_t rds_capture_target_blocks;
extern FuriThread* rds_capture_writer_thread;

/** Stop capture and join writer when idle. */
void fred_fm_rds_capture_stop(void);

/** Signal writer thread to exit and wait. */
void fred_fm_rds_capture_writer_stop(void);

/** Start background SD writer thread. */
bool fred_fm_rds_capture_writer_start(void);

/** Enqueue one block for writer (deferred path). */
void fred_fm_rds_capture_write_block(const uint16_t* samples, size_t count);

/** Flush ring buffer to SD and write metadata. */
void fred_fm_rds_capture_flush_to_sd(void);

/** Begin capture session (allocate ring, preallocate file). */
void fred_fm_rds_capture_start(void);

/** ISR-safe capture hook; writes to ring or drops on overflow. */
bool fred_fm_rds_acquisition_realtime_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);
