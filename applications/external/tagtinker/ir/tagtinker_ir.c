/*
 * IR transmitter.
 *
 * TIM1 CH3N drives the built-in IR LED carrier.
 * DWT->CYCCCNT handles the symbol timing so we do not need another timer.
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
#define CARRIER_TIM TIM1
#define CARRIER_ARR (51 - 1)
#define CARRIER_CCR 25

/*
 * PP4 sends two bits per symbol. The gap selects the symbol value.
 * These are pre-computed CPU cycle counts at 64 MHz to avoid any
 * per-call overhead in the tight timing loop.
 */
#define PP4_BURST_CYCLES 2581
static const uint32_t pp4_gap_cycles[4] = {
    3871, /* symbol 0 ~60 us  */
    15483, /* symbol 3 ~242 us */
    7741, /* symbol 2 ~121 us */
    11612, /* symbol 1 ~181 us */
};

static bool ir_initialized = false;
static volatile bool ir_stop_requested = false;

static inline void carrier_on(void) {
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_0); /* PWM2 */
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void carrier_off(void) {
    uint32_t ccmr2 = CARRIER_TIM->CCMR2;
    ccmr2 &= ~(TIM_CCMR2_OC3M);
    ccmr2 |= TIM_CCMR2_OC3M_2; /* Force inactive */
    CARRIER_TIM->CCMR2 = ccmr2;
}

static inline void delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < cycles) {
    }
}

static void send_frame_pp4(const uint8_t* data, size_t len) {
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
    /* Final closing burst */
    carrier_on();
    delay_cycles(PP4_BURST_CYCLES);
    carrier_off();
}

void tagtinker_ir_init(void) {
    if(ir_initialized) return;

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
}

bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats_raw, uint8_t delay) {
    if(!ir_initialized) return false;
    if(!data || len == 0 || len > 255) return false;

    ir_stop_requested = false;
    uint32_t repeats = repeats_raw & 0x7FFF;

    for(uint32_t rep = 0; rep <= repeats; rep++) {
        if(ir_stop_requested) {
            carrier_off();
            return false;
        }

        /*
         * FURI_CRITICAL_ENTER/EXIT wraps each individual frame to prevent
         * OS interrupts from breaking the microsecond-level IR symbol timing.
         * The critical section is short (~1-2ms per frame) so it won't stall
         * the OS noticeably, and we yield via furi_delay_ms between repeats.
         */
        FURI_CRITICAL_ENTER();
        send_frame_pp4(data, len);
        FURI_CRITICAL_EXIT();

        if(rep < repeats) {
            /* Short gap between repeats using cycle-accurate delay.
             * delay parameter = gap in units of 500µs.
             * Always yield to OS every 5 repeats to prevent watchdog starvation. */
            if(delay > 0) {
                uint32_t gap_cycles = (uint32_t)delay * 32000U; /* 500µs per unit at 64MHz */
                delay_cycles(gap_cycles);
            }
            /* Yield to OS periodically */
            if((rep % 5U) == 4U) furi_delay_ms(1);
        }
    }

    return true;
}

void tagtinker_ir_stop(void) {
    ir_stop_requested = true;
    carrier_off();
}
