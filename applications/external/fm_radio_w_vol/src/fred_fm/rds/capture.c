/**
 * @file capture.c
 * @brief Raw ADC capture to SD card.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/core/core.h"
#include "src/fred_fm/rds/rds.h"
#include "src/fred_fm/rds/capture.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>
#include "src/drivers/rds/RDSAcquisition.h"

#ifdef ENABLE_RDS
#if ENABLE_ADC_CAPTURE
/* Ring buffer in RAM; writer thread streams blocks to SD. */
#define RDS_CAPTURE_FILE_TEMPLATE      APP_DATA_PATH("rds_capture_u16le_%04lu.raw")
#define RDS_CAPTURE_META_FILE_TEMPLATE APP_DATA_PATH("rds_capture_meta_%04lu.txt")
#define RDS_CAPTURE_PATH_MAX           128U
#define RDS_CAPTURE_MAX_INDEX          9999U
#define RDS_CAPTURE_SAMPLE_BYTES       sizeof(uint16_t)
#define RDS_CAPTURE_SAMPLE_BITS        16U
#define RDS_CAPTURE_SAMPLE_FORMAT      "u16le"
#define RDS_CAPTURE_TARGET_BLOCKS      512U
#define RDS_CAPTURE_TARGET_SAMPLES     (RDS_CAPTURE_TARGET_BLOCKS * RDS_ACQ_BLOCK_SAMPLES)
#define RDS_CAPTURE_TARGET_BYTES       (RDS_CAPTURE_TARGET_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES)
#define RDS_CAPTURE_RING_MIN_BLOCKS    8U
#define RDS_CAPTURE_RING_MAX_BLOCKS    128U
#define RDS_CAPTURE_HEAP_RESERVE_BYTES (16U * 1024U)
#define RDS_CAPTURE_WRITER_STACK_SIZE  2048U
#define RDS_CAPTURE_WRITER_PRIORITY    FuriThreadPriorityNormal
#define RDS_CAPTURE_WRITER_FLAG_WORK   (1U << 0)
#define RDS_CAPTURE_WRITER_FLAG_STOP   (1U << 1)

volatile bool rds_capture_active = false;
volatile bool rds_capture_requested = false;
volatile bool rds_capture_finalize_pending = false;
volatile bool rds_capture_abort_pending = false;
volatile bool rds_capture_error = false;
volatile bool rds_capture_complete = false;
uint16_t* rds_capture_ring = NULL;
uint32_t rds_capture_ring_capacity_blocks = 0U;
uint32_t rds_capture_ring_head_block = 0U;
uint32_t rds_capture_ring_tail_block = 0U;
uint32_t rds_capture_ring_count_blocks = 0U;
uint32_t rds_capture_ring_peak_blocks = 0U;
uint32_t rds_capture_target_blocks = RDS_CAPTURE_TARGET_BLOCKS;
uint32_t rds_capture_captured_blocks = 0U;
uint32_t rds_capture_written_blocks = 0U;
uint32_t rds_capture_capture_samples = 0U;
uint32_t rds_capture_ring_overflow_blocks = 0U;
uint32_t rds_capture_storage_write_errors = 0U;
uint32_t rds_capture_free_heap_bytes = 0U;
uint32_t rds_capture_write_call_count = 0U;
uint32_t rds_capture_write_total_ms = 0U;
uint32_t rds_capture_write_max_ms = 0U;
uint32_t rds_capture_write_max_bytes = 0U;
uint32_t rds_capture_write_total_bytes = 0U;
uint16_t rds_capture_min = 0U;
uint16_t rds_capture_max = 0U;
uint32_t rds_capture_clip_4095 = 0U;
uint64_t rds_capture_sum = 0U;
bool rds_capture_stats_valid = false;
RdsAcquisitionStats rds_capture_acq_start_stats;
uint32_t rds_capture_start_tick = 0U;
uint32_t rds_capture_stop_tick = 0U;
uint16_t rds_capture_pending_peak_blocks = 0U;
File* rds_capture_file = NULL;
Storage* rds_capture_storage = NULL;
FuriThread* rds_capture_writer_thread = NULL;
char rds_capture_file_path[RDS_CAPTURE_PATH_MAX] = {0};
char rds_capture_meta_file_path[RDS_CAPTURE_PATH_MAX] = {0};
uint32_t rds_capture_file_index = 0U;

void fred_fm_rds_capture_signal_writer(uint32_t flags) {
    if(!rds_capture_writer_thread) return;
    FuriThreadId thread_id = furi_thread_get_id(rds_capture_writer_thread);
    if(thread_id) {
        furi_thread_flags_set(thread_id, flags);
    }
}

void fred_fm_rds_capture_close_file(void) {
    if(rds_capture_file) {
        storage_file_close(rds_capture_file);
        storage_file_free(rds_capture_file);
        rds_capture_file = NULL;
    }
    if(rds_capture_storage) {
        furi_record_close(RECORD_STORAGE);
        rds_capture_storage = NULL;
    }
}

bool fred_fm_rds_capture_path_exists(Storage* storage, const char* path) {
    bool exists = false;

    if(!storage || !path || path[0] == '\0') {
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        return false;
    }

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        exists = true;
        storage_file_close(file);
    }

    storage_file_free(file);
    return exists;
}

bool fred_fm_rds_capture_prepare_paths(Storage* storage) {
    if(!storage) {
        return false;
    }

    for(uint32_t index = 1U; index <= RDS_CAPTURE_MAX_INDEX; index++) {
        int raw_len = snprintf(
            rds_capture_file_path,
            sizeof(rds_capture_file_path),
            RDS_CAPTURE_FILE_TEMPLATE,
            (unsigned long)index);
        int meta_len = snprintf(
            rds_capture_meta_file_path,
            sizeof(rds_capture_meta_file_path),
            RDS_CAPTURE_META_FILE_TEMPLATE,
            (unsigned long)index);

        if(raw_len <= 0 || meta_len <= 0 || raw_len >= (int)sizeof(rds_capture_file_path) ||
           meta_len >= (int)sizeof(rds_capture_meta_file_path)) {
            rds_capture_file_path[0] = '\0';
            rds_capture_meta_file_path[0] = '\0';
            return false;
        }

        if(!fred_fm_rds_capture_path_exists(storage, rds_capture_file_path) &&
           !fred_fm_rds_capture_path_exists(storage, rds_capture_meta_file_path)) {
            rds_capture_file_index = index;
            return true;
        }
    }

    rds_capture_file_path[0] = '\0';
    rds_capture_meta_file_path[0] = '\0';
    return false;
}

bool fred_fm_rds_capture_preallocate_file(void) {
    if(!rds_capture_file) return false;

    const uint8_t zero = 0U;
    const uint32_t last_byte_offset = (uint32_t)RDS_CAPTURE_TARGET_BYTES - 1U;

    if(!storage_file_seek(rds_capture_file, last_byte_offset, true)) {
        return false;
    }
    if(storage_file_write(rds_capture_file, &zero, sizeof(zero)) != sizeof(zero)) {
        return false;
    }
    if(!storage_file_seek(rds_capture_file, 0U, true)) {
        return false;
    }

    return true;
}

bool fred_fm_rds_capture_trim_file_to_written_size(void) {
    if(!rds_capture_file) return false;

    const uint32_t bytes_written =
        rds_capture_written_blocks * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;

    /* Skip truncate when capture filled the preallocated file. */
    if(bytes_written >= RDS_CAPTURE_TARGET_BYTES) {
        return true;
    }

    if(!storage_file_seek(rds_capture_file, bytes_written, true)) {
        return false;
    }
    if(!storage_file_truncate(rds_capture_file)) {
        return false;
    }

    return true;
}

uint32_t fred_fm_rds_capture_select_ring_blocks(void) {
    const size_t block_bytes = RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
    size_t free_heap = memmgr_get_free_heap();
    size_t usable_heap = 0U;
    uint32_t blocks = RDS_CAPTURE_RING_MIN_BLOCKS;

    rds_capture_free_heap_bytes = (uint32_t)free_heap;

    if(free_heap > RDS_CAPTURE_HEAP_RESERVE_BYTES) {
        usable_heap = free_heap - RDS_CAPTURE_HEAP_RESERVE_BYTES;
    }

    if(block_bytes > 0U) {
        size_t max_blocks = usable_heap / block_bytes;
        if(max_blocks > RDS_CAPTURE_RING_MAX_BLOCKS) {
            max_blocks = RDS_CAPTURE_RING_MAX_BLOCKS;
        }
        if(max_blocks > RDS_CAPTURE_TARGET_BLOCKS) {
            max_blocks = RDS_CAPTURE_TARGET_BLOCKS;
        }
        if(max_blocks >= RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks = (uint32_t)max_blocks;
        }
    }

    return blocks;
}

void fred_fm_rds_capture_release_ring(void) {
    if(rds_capture_ring) {
        free(rds_capture_ring);
        rds_capture_ring = NULL;
    }
    rds_capture_ring_capacity_blocks = 0U;
    rds_capture_ring_head_block = 0U;
    rds_capture_ring_tail_block = 0U;
    rds_capture_ring_count_blocks = 0U;
    rds_capture_ring_peak_blocks = 0U;
}

void fred_fm_rds_capture_clear_state_fields(void) {
    rds_capture_active = false;
    rds_capture_requested = false;
    rds_capture_finalize_pending = false;
    rds_capture_abort_pending = false;
    rds_capture_error = false;
    rds_capture_complete = false;
    rds_capture_target_blocks = RDS_CAPTURE_TARGET_BLOCKS;
    rds_capture_captured_blocks = 0U;
    rds_capture_written_blocks = 0U;
    rds_capture_capture_samples = 0U;
    rds_capture_ring_overflow_blocks = 0U;
    rds_capture_storage_write_errors = 0U;
    rds_capture_free_heap_bytes = 0U;
    rds_capture_write_call_count = 0U;
    rds_capture_write_total_ms = 0U;
    rds_capture_write_max_ms = 0U;
    rds_capture_write_max_bytes = 0U;
    rds_capture_write_total_bytes = 0U;
    rds_capture_min = 0U;
    rds_capture_max = 0U;
    rds_capture_clip_4095 = 0U;
    rds_capture_sum = 0U;
    rds_capture_stats_valid = false;
    memset(&rds_capture_acq_start_stats, 0, sizeof(rds_capture_acq_start_stats));
    rds_capture_start_tick = 0U;
    rds_capture_stop_tick = 0U;
    rds_capture_pending_peak_blocks = 0U;
    rds_capture_file_path[0] = '\0';
    rds_capture_meta_file_path[0] = '\0';
    rds_capture_file_index = 0U;
}

void fred_fm_rds_capture_reset_state(void) {
    fred_fm_rds_capture_release_ring();
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_clear_state_fields();
}

void fred_fm_rds_capture_update_acq_observed_stats(void) {
    RdsAcquisitionStats stats;
    rds_acquisition_get_stats(&rds_acquisition, &stats);
    if(stats.pending_blocks > rds_capture_pending_peak_blocks) {
        rds_capture_pending_peak_blocks = stats.pending_blocks;
    }
}

bool fred_fm_rds_capture_ring_write_block(const uint16_t* samples, size_t count) {
    if(!rds_capture_ring || count != RDS_ACQ_BLOCK_SAMPLES) return false;

    uint32_t offset_bytes = 0U;
    {
        FURI_CRITICAL_ENTER();
        if(rds_capture_ring_count_blocks >= rds_capture_ring_capacity_blocks) {
            FURI_CRITICAL_EXIT();
            rds_capture_ring_overflow_blocks++;
            return false;
        }
        offset_bytes =
            rds_capture_ring_head_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
        FURI_CRITICAL_EXIT();
    }

    memcpy((uint8_t*)rds_capture_ring + offset_bytes, samples, count * sizeof(uint16_t));

    {
        FURI_CRITICAL_ENTER();
        rds_capture_ring_head_block++;
        if(rds_capture_ring_head_block >= rds_capture_ring_capacity_blocks) {
            rds_capture_ring_head_block = 0U;
        }

        rds_capture_ring_count_blocks++;
        if(rds_capture_ring_count_blocks > rds_capture_ring_peak_blocks) {
            rds_capture_ring_peak_blocks = rds_capture_ring_count_blocks;
        }
        FURI_CRITICAL_EXIT();
    }

    return true;
}

bool fred_fm_rds_capture_ring_write_block_realtime(const uint16_t* samples, size_t count) {
    if(!rds_capture_ring || count != RDS_ACQ_BLOCK_SAMPLES) return false;
    if(rds_capture_ring_count_blocks >= rds_capture_ring_capacity_blocks) {
        rds_capture_ring_overflow_blocks++;
        return false;
    }

    uint32_t offset_bytes =
        rds_capture_ring_head_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
    memcpy((uint8_t*)rds_capture_ring + offset_bytes, samples, count * sizeof(uint16_t));

    rds_capture_ring_head_block++;
    if(rds_capture_ring_head_block >= rds_capture_ring_capacity_blocks) {
        rds_capture_ring_head_block = 0U;
    }

    rds_capture_ring_count_blocks++;
    if(rds_capture_ring_count_blocks > rds_capture_ring_peak_blocks) {
        rds_capture_ring_peak_blocks = rds_capture_ring_count_blocks;
    }

    return true;
}

bool fred_fm_rds_capture_ring_peek_blocks(uint32_t* offset_bytes, uint32_t* blocks) {
    if(!offset_bytes || !blocks) return false;

    bool has_blocks = false;
    FURI_CRITICAL_ENTER();
    if(rds_capture_ring && (rds_capture_ring_count_blocks > 0U)) {
        uint32_t contiguous_blocks =
            rds_capture_ring_capacity_blocks - rds_capture_ring_tail_block;
        if(contiguous_blocks > rds_capture_ring_count_blocks) {
            contiguous_blocks = rds_capture_ring_count_blocks;
        }

        *offset_bytes =
            rds_capture_ring_tail_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
        *blocks = contiguous_blocks;
        has_blocks = true;
    }
    FURI_CRITICAL_EXIT();

    return has_blocks;
}

void fred_fm_rds_capture_ring_consume_blocks(uint32_t blocks) {
    if(blocks == 0U) return;

    FURI_CRITICAL_ENTER();
    if(blocks > rds_capture_ring_count_blocks) {
        blocks = rds_capture_ring_count_blocks;
    }
    if(blocks > 0U) {
        rds_capture_ring_tail_block += blocks;
        if(rds_capture_ring_tail_block >= rds_capture_ring_capacity_blocks) {
            rds_capture_ring_tail_block %= rds_capture_ring_capacity_blocks;
        }
        rds_capture_ring_count_blocks -= blocks;
        rds_capture_written_blocks += blocks;
    }
    FURI_CRITICAL_EXIT();
}

void fred_fm_rds_capture_update_stats_block(const uint16_t* samples, size_t count) {
    if(!samples || count == 0U) return;

    for(size_t i = 0; i < count; i++) {
        uint16_t sample = samples[i];

        if(!rds_capture_stats_valid) {
            rds_capture_min = sample;
            rds_capture_max = sample;
            rds_capture_stats_valid = true;
        } else {
            if(sample < rds_capture_min) rds_capture_min = sample;
            if(sample > rds_capture_max) rds_capture_max = sample;
        }

        if(sample >= 4095U) {
            rds_capture_clip_4095++;
        }

        rds_capture_sum += sample;
    }

    rds_capture_capture_samples += (uint32_t)count;
}

void fred_fm_rds_capture_mark_done(void) {
    rds_capture_active = false;
    rds_capture_finalize_pending = true;
    rds_capture_stop_tick = furi_get_tick();
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
}

void fred_fm_rds_capture_mark_done_deferred(void) {
    rds_capture_active = false;
    rds_capture_finalize_pending = true;
}

/** Begin RAW capture: allocate ring, open SD file, arm writer. */
void fred_fm_rds_capture_start(void) {
    if(rds_capture_active || rds_capture_finalize_pending) return;
    if(!rds_capture_writer_thread) {
        FURI_LOG_W(TAG, "ADC capture: writer thread unavailable");
        return;
    }

    fred_fm_rds_capture_reset_state();

    uint32_t initial_blocks = fred_fm_rds_capture_select_ring_blocks();
    for(uint32_t blocks = initial_blocks; blocks >= 2U;) {
        rds_capture_ring = malloc(blocks * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES);
        if(rds_capture_ring) {
            rds_capture_ring_capacity_blocks = blocks;
            break;
        }

        if(blocks > 64U) {
            blocks -= 8U;
        } else if(blocks > 32U) {
            blocks -= 4U;
        } else if(blocks > 16U) {
            blocks -= 2U;
        } else if(blocks > RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks -= 1U;
        } else if(blocks == RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks = 4U;
        } else if(blocks == 4U) {
            blocks = 2U;
        } else {
            break;
        }
    }
    if(!rds_capture_ring) {
        FURI_LOG_W(TAG, "ADC capture: ring malloc failed");
        fred_fm_rds_capture_reset_state();
        return;
    }

    rds_capture_storage = furi_record_open(RECORD_STORAGE);
    if(!rds_capture_storage) {
        FURI_LOG_W(TAG, "ADC capture: storage unavailable");
        fred_fm_rds_capture_reset_state();
        return;
    }

    fred_fm_ensure_app_data_dir(rds_capture_storage);
    if(!fred_fm_rds_capture_prepare_paths(rds_capture_storage)) {
        FURI_LOG_W(TAG, "ADC capture: file path allocation failed");
        fred_fm_rds_capture_reset_state();
        return;
    }
    rds_capture_file = storage_file_alloc(rds_capture_storage);
    if(!rds_capture_file ||
       !storage_file_open(
           rds_capture_file, rds_capture_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "ADC capture: raw open failed");
        fred_fm_rds_capture_reset_state();
        return;
    }
    if(!fred_fm_rds_capture_preallocate_file()) {
        FURI_LOG_W(TAG, "ADC capture: raw prealloc failed");
        fred_fm_rds_capture_reset_state();
        return;
    }

    rds_acquisition_get_stats(&rds_acquisition, &rds_capture_acq_start_stats);
    rds_capture_start_tick = furi_get_tick();
    rds_capture_stop_tick = rds_capture_start_tick;
    fred_fm_rds_capture_update_acq_observed_stats();

    rds_capture_active = true;
    rds_capture_abort_pending = false;
    FURI_LOG_I(
        TAG,
        "ADC capture started #%lu (%lu blocks ring, %lu KiB free heap, %lu blocks target)",
        (unsigned long)rds_capture_file_index,
        (unsigned long)rds_capture_ring_capacity_blocks,
        (unsigned long)(rds_capture_free_heap_bytes / 1024U),
        (unsigned long)rds_capture_target_blocks);
}

/** Abort capture and free ring/file without finalizing SD output. */
void fred_fm_rds_capture_stop(void) {
    rds_capture_requested = false;

    if(rds_capture_active || rds_capture_finalize_pending || rds_capture_file ||
       rds_capture_ring) {
        rds_capture_active = false;
        rds_capture_finalize_pending = false;
        rds_capture_abort_pending = true;
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
        return;
    }

    fred_fm_rds_capture_reset_state();
}

/** ISR path: enqueue one ADC block into the capture ring. */
void fred_fm_rds_capture_write_block(const uint16_t* samples, size_t count) {
    if(!rds_capture_active || !rds_capture_ring) return;
    if(count != RDS_ACQ_BLOCK_SAMPLES) return;

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done();
        return;
    }

    if(!fred_fm_rds_capture_ring_write_block(samples, count)) {
        return;
    }

    rds_capture_captured_blocks++;
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done();
    }
}

bool fred_fm_rds_acquisition_realtime_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context) {
    UNUSED(adc_midpoint);
    UNUSED(context);

    if(fred_fm_app_exiting || rds_pipeline_stopping)
        return false; // Skip during exit / pipeline stop

    if(!rds_capture_active || !rds_capture_ring) return false;
    if(count != RDS_ACQ_BLOCK_SAMPLES) return false;

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done_deferred();
        return true;
    }

    if(!fred_fm_rds_capture_ring_write_block_realtime(samples, count)) {
        return false;
    }

    rds_capture_captured_blocks++;
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done_deferred();
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    }

    return true;
}

void fred_fm_rds_capture_write_meta(void) {
    if(!rds_capture_storage) return;
    if(rds_capture_meta_file_path[0] == '\0') return;

    File* meta = storage_file_alloc(rds_capture_storage);
    if(meta &&
       storage_file_open(meta, rds_capture_meta_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[128];
        const uint32_t ticks_per_second = furi_ms_to_ticks(1000U);
        RdsAcquisitionStats stats;
        rds_acquisition_get_stats(&rds_acquisition, &stats);
        uint32_t capture_elapsed_ticks = rds_capture_stop_tick - rds_capture_start_tick;
        uint32_t capture_elapsed_ms = 0U;
        uint32_t measured_sample_rate_hz = 0U;
        uint32_t dma_measured_sample_rate_hz = 0U;
        uint32_t write_avg_bytes = 0U;
        uint32_t write_effective_rate_bps = 0U;
        uint32_t dma_half_events =
            stats.dma_half_events - rds_capture_acq_start_stats.dma_half_events;
        uint32_t dma_full_events =
            stats.dma_full_events - rds_capture_acq_start_stats.dma_full_events;
        uint32_t total_dma_blocks =
            stats.total_dma_blocks - rds_capture_acq_start_stats.total_dma_blocks;
        uint32_t delivered_blocks =
            stats.delivered_blocks - rds_capture_acq_start_stats.delivered_blocks;
        uint32_t dropped_blocks =
            stats.dropped_blocks - rds_capture_acq_start_stats.dropped_blocks;
        uint32_t adc_overrun_count =
            stats.adc_overrun_count - rds_capture_acq_start_stats.adc_overrun_count;
        uint32_t capture_mean_x100 =
            (rds_capture_capture_samples > 0U) ?
                (uint32_t)((rds_capture_sum * 100ULL) / rds_capture_capture_samples) :
                0U;
        int n;

        if((capture_elapsed_ticks > 0U) && (ticks_per_second > 0U)) {
            capture_elapsed_ms =
                (uint32_t)(((uint64_t)capture_elapsed_ticks * 1000ULL) / ticks_per_second);
            measured_sample_rate_hz =
                (uint32_t)(((uint64_t)rds_capture_capture_samples * (uint64_t)ticks_per_second) /
                           (uint64_t)capture_elapsed_ticks);
            dma_measured_sample_rate_hz =
                (uint32_t)(((uint64_t)total_dma_blocks * (uint64_t)RDS_ACQ_BLOCK_SAMPLES *
                            (uint64_t)ticks_per_second) /
                           (uint64_t)capture_elapsed_ticks);
        }
        if(rds_capture_write_call_count > 0U) {
            write_avg_bytes = rds_capture_write_total_bytes / rds_capture_write_call_count;
        }
        if(rds_capture_write_total_ms > 0U) {
            write_effective_rate_bps =
                (uint32_t)(((uint64_t)rds_capture_write_total_bytes * 1000ULL) /
                           (uint64_t)rds_capture_write_total_ms);
        }

#define CAP_META(fmt, ...)                                \
    n = snprintf(line, sizeof(line), fmt, ##__VA_ARGS__); \
    if(n > 0) storage_file_write(meta, line, (size_t)n)

        CAP_META("capture_index=%lu\n", (unsigned long)rds_capture_file_index);
        CAP_META("capture_file=%s\n", rds_capture_file_path);
        CAP_META("capture_meta_file=%s\n", rds_capture_meta_file_path);
        CAP_META("capture_samples=%lu\n", (unsigned long)rds_capture_capture_samples);
        CAP_META("capture_target_samples=%lu\n", (unsigned long)RDS_CAPTURE_TARGET_SAMPLES);
        CAP_META("capture_target_blocks=%lu\n", (unsigned long)rds_capture_target_blocks);
        CAP_META("capture_blocks=%lu\n", (unsigned long)rds_capture_captured_blocks);
        CAP_META("written_blocks=%lu\n", (unsigned long)rds_capture_written_blocks);
        CAP_META("capture_elapsed_ms=%lu\n", (unsigned long)capture_elapsed_ms);
        CAP_META(
            "configured_sample_rate_hz=%lu\n", (unsigned long)stats.configured_sample_rate_hz);
        CAP_META("measured_sample_rate_hz=%lu\n", (unsigned long)measured_sample_rate_hz);
        CAP_META("raw_effective_sample_rate_hz=%lu\n", (unsigned long)measured_sample_rate_hz);
        CAP_META("sample_format=%s\n", RDS_CAPTURE_SAMPLE_FORMAT);
        CAP_META("sample_bits=%u\n", (unsigned)RDS_CAPTURE_SAMPLE_BITS);
        CAP_META("sample_bytes=%u\n", (unsigned)RDS_CAPTURE_SAMPLE_BYTES);
        CAP_META("dma_measured_sample_rate_hz=%lu\n", (unsigned long)dma_measured_sample_rate_hz);
        CAP_META("adc_midpoint=%u\n", (unsigned)stats.adc_midpoint);
        CAP_META("adc_min=%u\n", (unsigned)rds_capture_min);
        CAP_META("adc_max=%u\n", (unsigned)rds_capture_max);
        CAP_META(
            "adc_mean=%lu.%02lu\n",
            (unsigned long)(capture_mean_x100 / 100U),
            (unsigned long)(capture_mean_x100 % 100U));
        CAP_META("adc_clip_4095=%lu\n", (unsigned long)rds_capture_clip_4095);
        CAP_META("tuned_freq_10khz=%lu\n", (unsigned long)fred_fm_get_current_freq_10khz());
        CAP_META(
            "manual_carrier_offset_centihz=%ld\n", (long)fred_fm_rds_get_manual_offset_centihz());
        CAP_META("ring_capacity_blocks=%lu\n", (unsigned long)rds_capture_ring_capacity_blocks);
        CAP_META(
            "ring_capacity_bytes=%lu\n",
            (unsigned long)(rds_capture_ring_capacity_blocks * RDS_ACQ_BLOCK_SAMPLES *
                            RDS_CAPTURE_SAMPLE_BYTES));
        CAP_META("free_heap_bytes=%lu\n", (unsigned long)rds_capture_free_heap_bytes);
        CAP_META("ring_peak_blocks=%lu\n", (unsigned long)rds_capture_ring_peak_blocks);
        CAP_META("ring_overflow_blocks=%lu\n", (unsigned long)rds_capture_ring_overflow_blocks);
        CAP_META("write_call_count=%lu\n", (unsigned long)rds_capture_write_call_count);
        CAP_META("write_total_ms=%lu\n", (unsigned long)rds_capture_write_total_ms);
        CAP_META("write_max_ms=%lu\n", (unsigned long)rds_capture_write_max_ms);
        CAP_META("write_total_bytes=%lu\n", (unsigned long)rds_capture_write_total_bytes);
        CAP_META("write_max_bytes=%lu\n", (unsigned long)rds_capture_write_max_bytes);
        CAP_META("write_avg_bytes=%lu\n", (unsigned long)write_avg_bytes);
        CAP_META("write_effective_rate_bps=%lu\n", (unsigned long)write_effective_rate_bps);
        CAP_META("storage_write_errors=%lu\n", (unsigned long)rds_capture_storage_write_errors);
        CAP_META("capture_complete=%u\n", rds_capture_complete ? 1U : 0U);
        CAP_META("capture_error=%u\n", rds_capture_error ? 1U : 0U);
        CAP_META("dma_buffer_samples=%u\n", (unsigned)stats.dma_buffer_samples);
        CAP_META("dma_block_samples=%u\n", (unsigned)stats.block_samples);
        CAP_META("dma_half_events=%lu\n", (unsigned long)dma_half_events);
        CAP_META("dma_full_events=%lu\n", (unsigned long)dma_full_events);
        CAP_META("total_dma_blocks=%lu\n", (unsigned long)total_dma_blocks);
        CAP_META("delivered_blocks=%lu\n", (unsigned long)delivered_blocks);
        CAP_META("pending_blocks=%u\n", (unsigned)stats.pending_blocks);
        CAP_META("pending_peak_blocks=%u\n", (unsigned)rds_capture_pending_peak_blocks);
        CAP_META("dropped_blocks=%lu\n", (unsigned long)dropped_blocks);
        CAP_META("adc_overrun_count=%lu\n", (unsigned long)adc_overrun_count);
#undef CAP_META
        storage_file_close(meta);
    }
    if(meta) storage_file_free(meta);
}

void fred_fm_rds_capture_finish(void) {
    const bool target_reached = (rds_capture_captured_blocks >= rds_capture_target_blocks) &&
                                (rds_capture_written_blocks >= rds_capture_target_blocks);

    rds_capture_complete = !rds_capture_error && target_reached;

    if(!fred_fm_rds_capture_trim_file_to_written_size()) {
        rds_capture_storage_write_errors++;
        rds_capture_error = true;
        rds_capture_complete = false;
    }

    fred_fm_rds_capture_write_meta();
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_release_ring();
    rds_capture_finalize_pending = false;
    rds_capture_abort_pending = false;

    FURI_LOG_I(
        TAG,
        "ADC capture finished (%lu/%lu blocks, err=%u)",
        (unsigned long)rds_capture_written_blocks,
        (unsigned long)rds_capture_target_blocks,
        rds_capture_error ? 1U : 0U);

    /* Main thread owns ADC stop during pipeline teardown (avoid double-free). */
    if(!fred_fm_rds_pipeline_enabled() && !rds_pipeline_stopping && !fred_fm_app_exiting) {
        fred_fm_rds_timer_stop();
        fred_fm_rds_adc_stop();
    }
}

void fred_fm_rds_capture_abort_cleanup(void) {
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_release_ring();
    fred_fm_rds_capture_clear_state_fields();
}

int32_t fred_fm_rds_capture_writer_thread_callback(void* context) {
    UNUSED(context);

    while(true) {
        uint32_t flags = furi_thread_flags_wait(
            RDS_CAPTURE_WRITER_FLAG_WORK | RDS_CAPTURE_WRITER_FLAG_STOP,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(flags & FuriFlagError) {
            continue;
        }

        if(flags & RDS_CAPTURE_WRITER_FLAG_STOP) {
            break;
        }

        while(true) {
            if(fred_fm_app_exiting) {
                break;
            }
            if(rds_capture_abort_pending) {
                fred_fm_rds_capture_abort_cleanup();
                break;
            }

            uint32_t offset_bytes = 0U;
            uint32_t blocks = 0U;
            if(fred_fm_rds_capture_ring_peek_blocks(&offset_bytes, &blocks)) {
                if(!rds_capture_file || !rds_capture_ring) {
                    rds_capture_error = true;
                    fred_fm_rds_capture_abort_cleanup();
                    break;
                }

                size_t sample_count = (size_t)blocks * RDS_ACQ_BLOCK_SAMPLES;
                size_t bytes = sample_count * RDS_CAPTURE_SAMPLE_BYTES;
                uint32_t write_start_tick = furi_get_tick();
                size_t written = storage_file_write(
                    rds_capture_file, (const uint8_t*)rds_capture_ring + offset_bytes, bytes);
                uint32_t write_elapsed_ms = furi_get_tick() - write_start_tick;
                if(written != bytes) {
                    rds_capture_storage_write_errors++;
                    rds_capture_error = true;
                    rds_capture_complete = false;
                    fred_fm_rds_capture_abort_cleanup();
                    break;
                }

                rds_capture_write_call_count++;
                rds_capture_write_total_ms += write_elapsed_ms;
                rds_capture_write_total_bytes += (uint32_t)bytes;
                if(write_elapsed_ms > rds_capture_write_max_ms) {
                    rds_capture_write_max_ms = write_elapsed_ms;
                }
                if(bytes > rds_capture_write_max_bytes) {
                    rds_capture_write_max_bytes = (uint32_t)bytes;
                }

                fred_fm_rds_capture_update_stats_block(
                    (const uint16_t*)((const uint8_t*)rds_capture_ring + offset_bytes),
                    sample_count);
                fred_fm_rds_capture_ring_consume_blocks(blocks);
                continue;
            }

            if(rds_capture_finalize_pending) {
                fred_fm_rds_capture_finish();
                break;
            }

            break;
        }
    }

    if(rds_capture_file || rds_capture_ring || rds_capture_abort_pending || rds_capture_active ||
       rds_capture_finalize_pending) {
        fred_fm_rds_capture_abort_cleanup();
    }

    return 0;
}

bool fred_fm_rds_capture_writer_start(void) {
    if(rds_capture_writer_thread) return true;

    rds_capture_writer_thread = furi_thread_alloc_ex(
        "RdsCaptureWrite",
        RDS_CAPTURE_WRITER_STACK_SIZE,
        fred_fm_rds_capture_writer_thread_callback,
        NULL);
    if(!rds_capture_writer_thread) {
        FURI_LOG_W(TAG, "ADC capture: writer thread alloc failed");
        return false;
    }

    furi_thread_set_priority(rds_capture_writer_thread, RDS_CAPTURE_WRITER_PRIORITY);
    furi_thread_start(rds_capture_writer_thread);
    return true;
}

void fred_fm_rds_capture_writer_stop(void) {
    if(!rds_capture_writer_thread) return;

    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK | RDS_CAPTURE_WRITER_FLAG_STOP);
    furi_thread_join(rds_capture_writer_thread);
    furi_thread_free(rds_capture_writer_thread);
    rds_capture_writer_thread = NULL;
}

/** Wake writer thread to drain ring or finalize (no SD I/O here). */
void fred_fm_rds_capture_flush_to_sd(void) {
    if(rds_capture_active || rds_capture_finalize_pending) {
        fred_fm_rds_capture_update_acq_observed_stats();
    }

    if(rds_capture_finalize_pending && !rds_capture_active &&
       (rds_capture_stop_tick == rds_capture_start_tick)) {
        rds_capture_stop_tick = furi_get_tick();
    }

    if(rds_capture_ring_count_blocks > 0U || rds_capture_finalize_pending ||
       rds_capture_abort_pending) {
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    }
}

#else /* !ENABLE_ADC_CAPTURE */
inline void fred_fm_rds_capture_stop(void) {
}
inline void fred_fm_rds_capture_flush_to_sd(void) {
}
#endif /* ENABLE_ADC_CAPTURE */

#endif
