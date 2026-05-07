#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <furi.h>
#include <furi_hal.h>

#define RDS_DECODE_SAMPLE_RATE_HZ     125000U
#define RDS_ACQ_TARGET_SAMPLE_RATE_HZ RDS_DECODE_SAMPLE_RATE_HZ
#define RDS_ACQ_DMA_BUFFER_SAMPLES    2048U
#define RDS_ACQ_BLOCK_SAMPLES         1024U
#define RDS_ACQ_RING_CAPACITY_BLOCKS  8U
#define RDS_ACQ_TIMER_MS              2U
#define RDS_ACQ_MAX_BLOCKS_PER_TICK   3U

typedef void (*RdsAcquisitionBlockCallback)(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);

typedef bool (*RdsAcquisitionRealtimeBlockCallback)(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);

typedef struct {
    uint32_t configured_sample_rate_hz;
    uint32_t measured_sample_rate_hz;
    uint16_t adc_midpoint;
    uint16_t dma_buffer_samples;
    uint16_t block_samples;
    uint32_t dma_half_events;
    uint32_t dma_full_events;
    uint32_t total_dma_blocks;
    uint32_t delivered_blocks;
    uint32_t dropped_blocks;
    uint16_t pending_blocks;
    uint16_t pending_peak_blocks;
    uint16_t ring_capacity_blocks;
    uint32_t ring_overrun_count;
    uint32_t adc_overrun_count;
    uint32_t samples_delivered;
    bool running;
} RdsAcquisitionStats;

typedef struct {
    const GpioPin* pin;
    FuriHalAdcChannel channel;
    FuriHalAdcHandle* adc_handle;
    uint16_t dma_buffer[RDS_ACQ_DMA_BUFFER_SAMPLES];
    RdsAcquisitionBlockCallback block_callback;
    void* callback_context;
    RdsAcquisitionRealtimeBlockCallback realtime_block_callback;
    void* realtime_callback_context;
    uint32_t start_tick;
    uint32_t last_tick;
    uint32_t sample_count;
    uint32_t timer_ticks;
    uint16_t pending_block_ring[RDS_ACQ_RING_CAPACITY_BLOCKS][RDS_ACQ_BLOCK_SAMPLES];
    volatile uint8_t pending_ring_head;
    volatile uint8_t pending_ring_tail;
    volatile uint16_t pending_ring_count;
    RdsAcquisitionStats stats;
} RdsAcquisition;

void rds_acquisition_init(
    RdsAcquisition* acquisition,
    const GpioPin* pin,
    FuriHalAdcChannel channel,
    uint16_t adc_midpoint,
    RdsAcquisitionBlockCallback block_callback,
    void* callback_context);
void rds_acquisition_set_realtime_block_callback(
    RdsAcquisition* acquisition,
    RdsAcquisitionRealtimeBlockCallback realtime_block_callback,
    void* realtime_callback_context);

void rds_acquisition_reset(RdsAcquisition* acquisition);
bool rds_acquisition_start(RdsAcquisition* acquisition);
void rds_acquisition_stop(RdsAcquisition* acquisition);
void rds_acquisition_on_timer_tick(RdsAcquisition* acquisition, bool drain_all_pending);
void rds_acquisition_get_stats(const RdsAcquisition* acquisition, RdsAcquisitionStats* out_stats);
