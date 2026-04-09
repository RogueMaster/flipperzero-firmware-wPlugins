/*
 * IR transmitter.
 *
 * TIM1 CH3N drives the built-in IR LED carrier.
 * DWT->CYCCNT handles the symbol timing so we do not need another timer.
 */

#include "tagtinker_ir.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>
#include <furi_hal_cortex.h>

#include <stm32wbxx_ll_tim.h>

/* Carrier setup for the built-in IR LED on TIM1 CH3N. */
#define CARRIER_TIM       TIM1
#define CARRIER_ARR       (51 - 1)
#define CARRIER_CCR       25

/* PP4 sends two bits per symbol. The gap selects the symbol value. */
#define PP4_BURST_CYCLES  2581
static const uint32_t pp4_gap_cycles[4] = {
    3871,
    15483,
    7741,
    11612,
};

/* PP16 sends four bits per symbol. */
#define PP16_BURST_CYCLES 1344
static const uint32_t pp16_gap_cycles[16] = {
    1728,
    3264,
    2240,
    2752,
    9408,
    7872,
    8896,
    8384,
    5312,
    3776,
    4800,
    4288,
    5824,
    7360,
    6336,
    6848
};

static bool ir_initialized = false;
static volatile bool ir_stop_requested = false;

static inline void carrier_on(void) {
    /* PWM2 on CH3N gives us the carrier burst on the built-in LED. */
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_0);
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void carrier_off(void) {
    /* Force-inactive stops the carrier between symbols. */
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= TIM_CCMR2_OC3M_2;
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < cycles) {
    }
}

static void send_frame_pp4(const uint8_t* data, size_t len) {
    /* PP4 walks each byte from least-significant bits upward, two bits at a time. */
    for(size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t current_byte = data[byte_idx];

        for(int sym = 0; sym < 4; sym++) {
            uint8_t symbol = current_byte & 0x03;
            current_byte >>= 2;

            carrier_on();
            delay_cycles(PP4_BURST_CYCLES);

            carrier_off();
            delay_cycles(pp4_gap_cycles[symbol]);
        }
    }

    carrier_on();
    delay_cycles(PP4_BURST_CYCLES);
    carrier_off();
}

static void send_frame_pp16(const uint8_t* data, size_t len) {
    /* PP16 uses the same pattern, but four bits per symbol. */
    for(size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t current_byte = data[byte_idx];

        for(int sym = 0; sym < 2; sym++) {
            uint8_t symbol = current_byte & 0x0F;
            current_byte >>= 4;

            carrier_on();
            delay_cycles(PP16_BURST_CYCLES);

            carrier_off();
            delay_cycles(pp16_gap_cycles[symbol]);
        }
    }

    carrier_on();
    delay_cycles(PP16_BURST_CYCLES);
    carrier_off();
}

void tagtinker_ir_init(void) {
    if(ir_initialized) return;

    /* Claim TIM1 from the stock IR stack before configuring our own carrier. */
    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) {
        furi_hal_bus_disable(FuriHalBusTIM1);
    }
    furi_hal_bus_enable(FuriHalBusTIM1);

    furi_hal_gpio_init_ex(
        &gpio_infrared_tx,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn1TIM1);

    LL_TIM_SetPrescaler(CARRIER_TIM, 0);
    LL_TIM_SetAutoReload(CARRIER_TIM, CARRIER_ARR);
    LL_TIM_SetCounter(CARRIER_TIM, 0);

    LL_TIM_OC_SetMode(CARRIER_TIM, LL_TIM_CHANNEL_CH3, LL_TIM_OCMODE_PWM2);
    LL_TIM_OC_SetCompareCH3(CARRIER_TIM, CARRIER_CCR);
    LL_TIM_OC_EnablePreload(CARRIER_TIM, LL_TIM_CHANNEL_CH3);

    LL_TIM_CC_EnableChannel(CARRIER_TIM, LL_TIM_CHANNEL_CH3N);
    LL_TIM_EnableAllOutputs(CARRIER_TIM);

    carrier_off();
    LL_TIM_EnableCounter(CARRIER_TIM);
    LL_TIM_GenerateEvent_UPDATE(CARRIER_TIM);

    ir_stop_requested = false;
    ir_initialized = true;

    FURI_LOG_I("TagTinker", "IR TX initialized: carrier %.3f MHz",
        (double)(64000000.0f / (CARRIER_ARR + 1) / 1000000.0f));
}

void tagtinker_ir_deinit(void) {
    if(!ir_initialized) return;

    tagtinker_ir_stop();

    carrier_off();
    LL_TIM_DisableAllOutputs(CARRIER_TIM);
    LL_TIM_CC_DisableChannel(CARRIER_TIM, LL_TIM_CHANNEL_CH3N);
    LL_TIM_DisableCounter(CARRIER_TIM);

    if(furi_hal_bus_is_enabled(FuriHalBusTIM1)) {
        furi_hal_bus_disable(FuriHalBusTIM1);
    }

    furi_hal_gpio_init(&gpio_infrared_tx, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    ir_initialized = false;
    FURI_LOG_I("TagTinker", "IR TX deinitialized");
}

bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats_raw, uint8_t delay) {
    if(!ir_initialized) return false;
    if(len == 0 || len > 255) return false;

    ir_stop_requested = false;

    bool is_pp16 = (repeats_raw & 0x8000) != 0;
    uint32_t repeats = repeats_raw & 0x7FFF;

    FURI_LOG_I("TagTinker", "TX start: %zu bytes, %lu repeats (PP%d), %u delay",
        len, repeats, is_pp16 ? 16 : 4, delay);

    for(uint32_t rep = 0; rep <= repeats; rep++) {
        if(ir_stop_requested) {
            FURI_LOG_I("TagTinker", "TX cancelled at repeat %lu", rep);
            carrier_off();
            return false;
        }

        if(is_pp16) {
            send_frame_pp16(data, len);
        } else {
            send_frame_pp4(data, len);
        }

        if(rep < repeats) {
            /* Delay units are 500 us to match the ESL timing tools. */
            uint32_t delay_us = (uint32_t)delay * 500;
            if(delay_us > 0) {
                uint32_t delay_ms_yield = delay_us / 1000;
                uint32_t delay_us_busy = delay_us % 1000;

                if(delay_ms_yield > 0) {
                    furi_delay_ms(delay_ms_yield);
                }

                if(delay_us_busy > 0) {
                    delay_cycles(delay_us_busy * 64);
                }
            }
        }

        if(((uint32_t)delay * 500) < 1000 && (rep % 10) == 9) {
            furi_delay_ms(1);
        }
    }

    FURI_LOG_I("TagTinker", "TX complete");
    return true;
}

bool tagtinker_ir_is_busy(void) {
    return false;
}

void tagtinker_ir_stop(void) {
    ir_stop_requested = true;
    carrier_off();
}
