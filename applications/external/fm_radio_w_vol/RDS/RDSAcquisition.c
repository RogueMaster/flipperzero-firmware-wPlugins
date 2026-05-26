#include "RDSAcquisition.h"

/* Real-time pipeline, 8.192 ms block cadence, ISR vs worker split: RDSAcquisition.h */

#include <string.h>

#include <stm32wbxx_ll_adc.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_dmamux.h>
#include <stm32wbxx_ll_tim.h>

#define RDS_ADC_DMA_INSTANCE         DMA1
#define RDS_ADC_DMA_CHANNEL          LL_DMA_CHANNEL_1
#define RDS_ADC_TRIGGER_TIMER        TIM1
#define RDS_ADC_TRIGGER_TIMER_CLK_HZ 64000000UL

typedef enum {
    RdsAcquisitionBlockNone = 0,
    RdsAcquisitionBlockHalf,
    RdsAcquisitionBlockFull,
} RdsAcquisitionBlockEvent;

static uint8_t rds_acquisition_ring_next(uint8_t index) {
    index++;
    if(index >= RDS_ACQ_RING_CAPACITY_BLOCKS) {
        index = 0U;
    }
    return index;
}

static uint32_t rds_acquisition_map_channel(FuriHalAdcChannel channel) {
    switch(channel) {
    case FuriHalAdcChannel0:
        return LL_ADC_CHANNEL_0;
    case FuriHalAdcChannel1:
        return LL_ADC_CHANNEL_1;
    case FuriHalAdcChannel2:
        return LL_ADC_CHANNEL_2;
    case FuriHalAdcChannel3:
        return LL_ADC_CHANNEL_3;
    case FuriHalAdcChannel4:
        return LL_ADC_CHANNEL_4;
    case FuriHalAdcChannel5:
        return LL_ADC_CHANNEL_5;
    case FuriHalAdcChannel6:
        return LL_ADC_CHANNEL_6;
    case FuriHalAdcChannel7:
        return LL_ADC_CHANNEL_7;
    case FuriHalAdcChannel8:
        return LL_ADC_CHANNEL_8;
    case FuriHalAdcChannel9:
        return LL_ADC_CHANNEL_9;
    case FuriHalAdcChannel10:
        return LL_ADC_CHANNEL_10;
    case FuriHalAdcChannel11:
        return LL_ADC_CHANNEL_11;
    case FuriHalAdcChannel12:
        return LL_ADC_CHANNEL_12;
    case FuriHalAdcChannel13:
        return LL_ADC_CHANNEL_13;
    case FuriHalAdcChannel14:
        return LL_ADC_CHANNEL_14;
    case FuriHalAdcChannel15:
        return LL_ADC_CHANNEL_15;
    case FuriHalAdcChannel16:
        return LL_ADC_CHANNEL_16;
    case FuriHalAdcChannel17:
    case FuriHalAdcChannelTEMPSENSOR:
        return LL_ADC_CHANNEL_TEMPSENSOR;
    case FuriHalAdcChannel18:
    case FuriHalAdcChannelVBAT:
        return LL_ADC_CHANNEL_VBAT;
    case FuriHalAdcChannelVREFINT:
        return LL_ADC_CHANNEL_VREFINT;
    case FuriHalAdcChannelNone:
    default:
        return LL_ADC_CHANNEL_9;
    }
}

static uint32_t rds_acquisition_configure_timer_rate(uint32_t target_rate_hz) {
    uint32_t divider = (RDS_ADC_TRIGGER_TIMER_CLK_HZ + (target_rate_hz / 2U)) / target_rate_hz;
    if(divider < 1U) divider = 1U;

    uint32_t auto_reload = divider - 1U;
    uint32_t actual_rate_hz = RDS_ADC_TRIGGER_TIMER_CLK_HZ / (auto_reload + 1U);

    if(!furi_hal_bus_is_enabled(FuriHalBusTIM1)) {
        furi_hal_bus_enable(FuriHalBusTIM1);
    }

    LL_TIM_DisableCounter(RDS_ADC_TRIGGER_TIMER);
    LL_TIM_SetPrescaler(RDS_ADC_TRIGGER_TIMER, 0U);
    LL_TIM_SetCounterMode(RDS_ADC_TRIGGER_TIMER, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(RDS_ADC_TRIGGER_TIMER, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetOnePulseMode(RDS_ADC_TRIGGER_TIMER, LL_TIM_ONEPULSEMODE_REPETITIVE);
    LL_TIM_SetUpdateSource(RDS_ADC_TRIGGER_TIMER, LL_TIM_UPDATESOURCE_REGULAR);
    LL_TIM_DisableARRPreload(RDS_ADC_TRIGGER_TIMER);
    LL_TIM_SetAutoReload(RDS_ADC_TRIGGER_TIMER, auto_reload);
    LL_TIM_SetCounter(RDS_ADC_TRIGGER_TIMER, 0U);
    LL_TIM_SetTriggerOutput(RDS_ADC_TRIGGER_TIMER, LL_TIM_TRGO_UPDATE);
    LL_TIM_GenerateEvent_UPDATE(RDS_ADC_TRIGGER_TIMER);

    return actual_rate_hz;
}

static void rds_acquisition_timer_start(void) {
    LL_TIM_SetCounter(RDS_ADC_TRIGGER_TIMER, 0U);
    LL_TIM_GenerateEvent_UPDATE(RDS_ADC_TRIGGER_TIMER);
    LL_TIM_EnableCounter(RDS_ADC_TRIGGER_TIMER);
}

static void rds_acquisition_timer_stop(void) {
    LL_TIM_DisableCounter(RDS_ADC_TRIGGER_TIMER);
}

static uint16_t rds_acquisition_pending_block_count(const RdsAcquisition* acquisition) {
    return acquisition->pending_ring_count;
}

static bool rds_acquisition_queue_block_copy(RdsAcquisition* acquisition, const uint16_t* block) {
    uint8_t slot;

    if(!acquisition || !block) {
        return false;
    }

    {
        FURI_CRITICAL_ENTER();
        if(acquisition->pending_ring_count >= RDS_ACQ_RING_CAPACITY_BLOCKS) {
            FURI_CRITICAL_EXIT();
            return false;
        }
        slot = acquisition->pending_ring_head;
        FURI_CRITICAL_EXIT();
    }

    memcpy(
        acquisition->pending_block_ring[slot],
        block,
        sizeof(acquisition->pending_block_ring[slot]));

    {
        FURI_CRITICAL_ENTER();
        acquisition->pending_ring_head = rds_acquisition_ring_next(slot);
        acquisition->pending_ring_count++;
        FURI_CRITICAL_EXIT();
    }
    return true;
}

static bool
    rds_acquisition_pop_pending_block_copy(RdsAcquisition* acquisition, uint16_t* out_block) {
    uint8_t slot;

    if(!acquisition || !out_block) {
        return false;
    }

    {
        FURI_CRITICAL_ENTER();
        if(acquisition->pending_ring_count == 0U) {
            FURI_CRITICAL_EXIT();
            return false;
        }
        slot = acquisition->pending_ring_tail;
        FURI_CRITICAL_EXIT();
    }

    memcpy(
        out_block,
        acquisition->pending_block_ring[slot],
        sizeof(acquisition->pending_block_ring[slot]));

    {
        FURI_CRITICAL_ENTER();
        acquisition->pending_ring_tail = rds_acquisition_ring_next(slot);
        acquisition->pending_ring_count--;
        FURI_CRITICAL_EXIT();
    }
    return true;
}

static void rds_acquisition_dma_disable(void) {
    LL_DMA_DisableIT_HT(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_DisableIT_TC(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_DisableIT_TE(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_DisableChannel(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);

    LL_DMA_ClearFlag_GI1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_HT1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_TC1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_TE1(RDS_ADC_DMA_INSTANCE);
}

static void rds_acquisition_dma_configure(RdsAcquisition* acquisition) {
    if(!furi_hal_bus_is_enabled(FuriHalBusDMA1)) {
        furi_hal_bus_enable(FuriHalBusDMA1);
    }
    if(!furi_hal_bus_is_enabled(FuriHalBusDMAMUX1)) {
        furi_hal_bus_enable(FuriHalBusDMAMUX1);
    }

    rds_acquisition_dma_disable();

    LL_DMA_SetPeriphRequest(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMAMUX_REQ_ADC1);
    LL_DMA_SetDataTransferDirection(
        RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetChannelPriorityLevel(
        RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetMode(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_MODE_CIRCULAR);
    LL_DMA_SetPeriphIncMode(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_SetMemorySize(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphAddress(
        RDS_ADC_DMA_INSTANCE,
        RDS_ADC_DMA_CHANNEL,
        LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA));
    LL_DMA_SetMemoryAddress(
        RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, (uint32_t)acquisition->dma_buffer);
    LL_DMA_SetDataLength(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL, RDS_ACQ_DMA_BUFFER_SAMPLES);

    LL_DMA_ClearFlag_GI1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_HT1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_TC1(RDS_ADC_DMA_INSTANCE);
    LL_DMA_ClearFlag_TE1(RDS_ADC_DMA_INSTANCE);

    LL_DMA_EnableIT_HT(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_EnableIT_TC(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_EnableIT_TE(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
    LL_DMA_EnableChannel(RDS_ADC_DMA_INSTANCE, RDS_ADC_DMA_CHANNEL);
}

static void rds_acquisition_adc_configure_regular_channel(RdsAcquisition* acquisition) {
    uint32_t ll_channel = rds_acquisition_map_channel(acquisition->channel);

    LL_ADC_REG_StopConversion(ADC1);
    LL_ADC_ClearFlag_OVR(ADC1);

    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_EXT_TIM1_TRGO);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, ll_channel);
}

static bool
    rds_acquisition_try_realtime_block(RdsAcquisition* acquisition, const uint16_t* block) {
    if(!acquisition || !acquisition->realtime_block_callback) return false;

    return acquisition->realtime_block_callback(
        block,
        RDS_ACQ_BLOCK_SAMPLES,
        acquisition->stats.adc_midpoint,
        acquisition->realtime_callback_context);
}

static void rds_acquisition_dma_isr(void* context) {
    RdsAcquisition* acquisition = context;
    if(!acquisition || !acquisition->stats.running) return;

    if(LL_DMA_IsActiveFlag_TE1(RDS_ADC_DMA_INSTANCE)) {
        LL_DMA_ClearFlag_TE1(RDS_ADC_DMA_INSTANCE);
        acquisition->stats.adc_overrun_count++;
    }

    if(LL_DMA_IsActiveFlag_HT1(RDS_ADC_DMA_INSTANCE)) {
        LL_DMA_ClearFlag_HT1(RDS_ADC_DMA_INSTANCE);
        acquisition->stats.dma_half_events++;
        acquisition->stats.total_dma_blocks++;
        if(rds_acquisition_try_realtime_block(acquisition, acquisition->dma_buffer)) {
            /* Raw capture mode consumes this block immediately from DMA. */
        } else if(rds_acquisition_queue_block_copy(acquisition, acquisition->dma_buffer)) {
        } else {
            acquisition->stats.dropped_blocks++;
            acquisition->stats.ring_overrun_count++;
        }
        uint16_t pending_blocks = rds_acquisition_pending_block_count(acquisition);
        if(pending_blocks > acquisition->stats.pending_peak_blocks) {
            acquisition->stats.pending_peak_blocks = pending_blocks;
        }
    }

    if(LL_DMA_IsActiveFlag_TC1(RDS_ADC_DMA_INSTANCE)) {
        LL_DMA_ClearFlag_TC1(RDS_ADC_DMA_INSTANCE);
        acquisition->stats.dma_full_events++;
        acquisition->stats.total_dma_blocks++;
        if(rds_acquisition_try_realtime_block(
               acquisition, &acquisition->dma_buffer[RDS_ACQ_BLOCK_SAMPLES])) {
            /* Raw capture mode consumes this block immediately from DMA. */
        } else if(rds_acquisition_queue_block_copy(
                      acquisition, &acquisition->dma_buffer[RDS_ACQ_BLOCK_SAMPLES])) {
        } else {
            acquisition->stats.dropped_blocks++;
            acquisition->stats.ring_overrun_count++;
        }
        uint16_t pending_blocks = rds_acquisition_pending_block_count(acquisition);
        if(pending_blocks > acquisition->stats.pending_peak_blocks) {
            acquisition->stats.pending_peak_blocks = pending_blocks;
        }
    }

    if(LL_ADC_IsActiveFlag_OVR(ADC1)) {
        LL_ADC_ClearFlag_OVR(ADC1);
        acquisition->stats.adc_overrun_count++;
    }
}

static void rds_acquisition_update_measured_rate(RdsAcquisition* acquisition) {
    uint32_t elapsed_ticks = acquisition->last_tick - acquisition->start_tick;
    if(elapsed_ticks == 0U) {
        acquisition->stats.measured_sample_rate_hz = 0U;
        return;
    }

    /* Use delivered_blocks (wraps after ~224 days) instead of sample_count
       (wraps after ~5.2 hours at 228 kHz) to avoid uint32_t overflow. */
    uint64_t total_samples =
        (uint64_t)acquisition->stats.delivered_blocks * (uint64_t)RDS_ACQ_BLOCK_SAMPLES;
    acquisition->stats.measured_sample_rate_hz =
        (uint32_t)((total_samples * (uint64_t)furi_ms_to_ticks(1000U)) / (uint64_t)elapsed_ticks);
}

void rds_acquisition_init(
    RdsAcquisition* acquisition,
    const GpioPin* pin,
    FuriHalAdcChannel channel,
    uint16_t adc_midpoint,
    RdsAcquisitionBlockCallback block_callback,
    void* callback_context) {
    if(!acquisition) return;

    memset(acquisition, 0, sizeof(*acquisition));
    acquisition->pin = pin;
    acquisition->channel = channel;
    acquisition->block_callback = block_callback;
    acquisition->callback_context = callback_context;

    acquisition->stats.configured_sample_rate_hz = RDS_ACQ_TARGET_SAMPLE_RATE_HZ;
    acquisition->stats.adc_midpoint = adc_midpoint;
    acquisition->stats.dma_buffer_samples = RDS_ACQ_DMA_BUFFER_SAMPLES;
    acquisition->stats.block_samples = RDS_ACQ_BLOCK_SAMPLES;
    acquisition->stats.ring_capacity_blocks = RDS_ACQ_RING_CAPACITY_BLOCKS;
}

void rds_acquisition_set_realtime_block_callback(
    RdsAcquisition* acquisition,
    RdsAcquisitionRealtimeBlockCallback realtime_block_callback,
    void* realtime_callback_context) {
    if(!acquisition) return;

    acquisition->realtime_block_callback = realtime_block_callback;
    acquisition->realtime_callback_context = realtime_callback_context;
}

void rds_acquisition_reset(RdsAcquisition* acquisition) {
    if(!acquisition) return;

    acquisition->start_tick = furi_get_tick();
    acquisition->last_tick = acquisition->start_tick;
    acquisition->sample_count = 0U;
    acquisition->timer_ticks = 0U;
    acquisition->pending_ring_head = 0U;
    acquisition->pending_ring_tail = 0U;
    acquisition->pending_ring_count = 0U;

    acquisition->stats.measured_sample_rate_hz = 0U;
    acquisition->stats.dma_half_events = 0U;
    acquisition->stats.dma_full_events = 0U;
    acquisition->stats.total_dma_blocks = 0U;
    acquisition->stats.delivered_blocks = 0U;
    acquisition->stats.dropped_blocks = 0U;
    acquisition->stats.pending_blocks = 0U;
    acquisition->stats.pending_peak_blocks = 0U;
    acquisition->stats.ring_overrun_count = 0U;
    acquisition->stats.adc_overrun_count = 0U;
}

bool rds_acquisition_start(RdsAcquisition* acquisition) {
    if(!acquisition) return false;
    if(acquisition->adc_handle) {
        acquisition->stats.running = true;
        return true;
    }

    furi_hal_gpio_init(acquisition->pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    acquisition->adc_handle = furi_hal_adc_acquire();
    if(!acquisition->adc_handle) {
        acquisition->stats.running = false;
        return false;
    }

    furi_hal_adc_configure_ex(
        acquisition->adc_handle,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversampleNone,
        FuriHalAdcSamplingtime12_5);

    rds_acquisition_reset(acquisition);
    acquisition->stats.configured_sample_rate_hz =
        rds_acquisition_configure_timer_rate(RDS_ACQ_TARGET_SAMPLE_RATE_HZ);

    rds_acquisition_adc_configure_regular_channel(acquisition);
    rds_acquisition_dma_configure(acquisition);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, rds_acquisition_dma_isr, acquisition);

    LL_ADC_REG_StartConversion(ADC1);
    rds_acquisition_timer_start();

    acquisition->stats.running = true;
    return true;
}

void rds_acquisition_stop(RdsAcquisition* acquisition) {
    if(!acquisition) return;

    acquisition->stats.running = false;
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch1, NULL, NULL);
    rds_acquisition_timer_stop();
    LL_ADC_REG_StopConversion(ADC1);
    rds_acquisition_dma_disable();

    if(acquisition->adc_handle) {
        furi_hal_adc_release(acquisition->adc_handle);
        acquisition->adc_handle = NULL;
    }
}

void rds_acquisition_on_timer_tick(RdsAcquisition* acquisition, bool drain_all_pending) {
    if(!acquisition || !acquisition->adc_handle || !acquisition->stats.running) return;

    acquisition->timer_ticks++;
    acquisition->last_tick = furi_get_tick();

    uint16_t block_copy[RDS_ACQ_BLOCK_SAMPLES];
    size_t delivered_blocks = 0U;
    size_t max_blocks_this_tick = SIZE_MAX;

    if(!drain_all_pending) {
        const uint16_t pending_before = rds_acquisition_pending_block_count(acquisition);
        max_blocks_this_tick = 1U;
        if(pending_before >= RDS_ACQ_RING_CAPACITY_BLOCKS) {
            max_blocks_this_tick = RDS_ACQ_MAX_BLOCKS_PER_TICK;
        } else if(pending_before > (RDS_ACQ_RING_CAPACITY_BLOCKS / 2U)) {
            max_blocks_this_tick = 2U;
        }
    }

    while(rds_acquisition_pop_pending_block_copy(acquisition, block_copy)) {
        acquisition->sample_count += RDS_ACQ_BLOCK_SAMPLES;
        acquisition->stats.delivered_blocks++;
        if(acquisition->block_callback) {
            acquisition->block_callback(
                block_copy,
                RDS_ACQ_BLOCK_SAMPLES,
                acquisition->stats.adc_midpoint,
                acquisition->callback_context);
        }

        delivered_blocks++;
        if(delivered_blocks >= max_blocks_this_tick) {
            break;
        }
    }

    acquisition->stats.pending_blocks = rds_acquisition_pending_block_count(acquisition);

    rds_acquisition_update_measured_rate(acquisition);
}

void rds_acquisition_get_stats(const RdsAcquisition* acquisition, RdsAcquisitionStats* out_stats) {
    if(!acquisition || !out_stats) return;
    *out_stats = acquisition->stats;
    out_stats->pending_blocks = rds_acquisition_pending_block_count(acquisition);
}
