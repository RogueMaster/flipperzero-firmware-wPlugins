/*
 * NOTE TO ANYONE USING THIS CODE
 *
 * The following is verbatim from the flipperzero-game-engine codebase and
 * exists with a separate license, GPL-3.0 
 *
 * While the flipper-gblink is BSD-2, this project is always open-source.
 *
 * If you are using the flipper-gblink library in another project that is not
 * open-source, then you should review the GPL-3.0 text to ensure you abide by
 * the terms of the license before releasing a binary made with this code and
 * not including the source alongside it.
 *
 */

#include "clock_timer_i.h"
#include <stdlib.h>

#include <furi_hal_interrupt.h>
#include <furi_hal_bus.h>
#include <stm32wbxx_ll_tim.h>

#define FURI_HAL_CLOCK_TIMER TIM2
#define FURI_HAL_CLOCK_TIMER_BUS FuriHalBusTIM2
#define FURI_HAL_CLOCK_TIMER_IRQ FuriHalInterruptIdTIM2

typedef struct {
    ClockTimerCallback callback;
    void* context;
} ClockTimer;

static ClockTimer clock_timer = {
    .callback = NULL,
    .context = NULL,
};

static void clock_timer_isr(void* context) {
    if(clock_timer.callback) {
        clock_timer.callback(context);
    }

    LL_TIM_ClearFlag_UPDATE(FURI_HAL_CLOCK_TIMER);
}

void clock_timer_start(ClockTimerCallback callback, void* context, float period) {
    clock_timer.callback = callback;
    clock_timer.context = context;

    furi_hal_bus_enable(FURI_HAL_CLOCK_TIMER_BUS);

    // init timer to produce interrupts
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    TIM_InitStruct.Autoreload = (SystemCoreClock / period) - 1;
    LL_TIM_Init(FURI_HAL_CLOCK_TIMER, &TIM_InitStruct);

    furi_hal_interrupt_set_isr(FURI_HAL_CLOCK_TIMER_IRQ, clock_timer_isr, clock_timer.context);

    LL_TIM_EnableIT_UPDATE(FURI_HAL_CLOCK_TIMER);
    LL_TIM_EnableCounter(FURI_HAL_CLOCK_TIMER);
}

void clock_timer_stop(void) {
    LL_TIM_DisableIT_UPDATE(FURI_HAL_CLOCK_TIMER);
    LL_TIM_DisableCounter(FURI_HAL_CLOCK_TIMER);

    furi_hal_bus_disable(FURI_HAL_CLOCK_TIMER_BUS);
    furi_hal_interrupt_set_isr(FURI_HAL_CLOCK_TIMER_IRQ, NULL, NULL);
}
