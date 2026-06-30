#include "ftdi_latency_timer.h"
#include "furi.h"
#include <furi_hal.h>
#include <furi_hal_bus.h>

#define TAG "FTDI_LATENCY_TIMER"

#define FTDI_LATENCY_TIMER_SPEED_DEFALUT 16 // ms defalut

struct FtdiLatencyTimer {
    uint8_t speed;
    bool enable;
    FtdiLatencyTimerCallbackUp callback;
    void* context;
};

static void ftdi_latency_timer_tim_isr(void* context);
static void ftdi_latency_timer_tim_init(FtdiLatencyTimer* ftdi_latency_timer);
static void ftdi_latency_timer_tim_deinit(FtdiLatencyTimer* ftdi_latency_timer);

FtdiLatencyTimer* ftdi_latency_timer_alloc(void) {
    FtdiLatencyTimer* ftdi_latency_timer = malloc(sizeof(FtdiLatencyTimer));
    ftdi_latency_timer->speed = FTDI_LATENCY_TIMER_SPEED_DEFALUT;
    ftdi_latency_timer->enable = false;

    ftdi_latency_timer_tim_init(ftdi_latency_timer);
    return ftdi_latency_timer;
}

void ftdi_latency_timer_free(FtdiLatencyTimer* ftdi_latency_timer) {
    if(!ftdi_latency_timer) return;
    ftdi_latency_timer->enable = false;
    ftdi_latency_timer_tim_deinit(ftdi_latency_timer);
    free(ftdi_latency_timer);
    ftdi_latency_timer = NULL;
}

void ftdi_latency_timer_set_callback(
    FtdiLatencyTimer* ftdi_latency_timer,
    FtdiLatencyTimerCallbackUp callback,
    void* context) {
    ftdi_latency_timer->callback = callback;
    ftdi_latency_timer->context = context;
}

void ftdi_latency_timer_enable(FtdiLatencyTimer* ftdi_latency_timer, bool enable) {
    ftdi_latency_timer->enable = enable;

    if(enable) {
        LL_TIM_SetCounter(TIM1, 0);
        LL_TIM_EnableCounter(TIM1);
    } else {
        LL_TIM_DisableCounter(TIM1);
    }
}

void ftdi_latency_timer_set_speed(FtdiLatencyTimer* ftdi_latency_timer, uint8_t speed) {
    UNUSED(ftdi_latency_timer);
    if(speed == 0) {
        speed = FTDI_LATENCY_TIMER_SPEED_DEFALUT;
        ftdi_latency_timer_enable(ftdi_latency_timer, false);
    } else {
        ftdi_latency_timer_enable(ftdi_latency_timer, true);
    }

    LL_TIM_SetPrescaler(TIM1, speed - 1);
}

uint8_t ftdi_latency_timer_get_speed(FtdiLatencyTimer* ftdi_latency_timer) {
    return ftdi_latency_timer->speed;
}

void ftdi_latency_timer_reset(FtdiLatencyTimer* ftdi_latency_timer) {
    UNUSED(ftdi_latency_timer);
    LL_TIM_SetCounter(TIM1, 0);
}

static void ftdi_latency_timer_tim_isr(void* context) {
    FtdiLatencyTimer* ftdi_latency_timer = context;
    UNUSED(ftdi_latency_timer);
    if(LL_TIM_IsActiveFlag_UPDATE(TIM1)) {
        LL_TIM_ClearFlag_UPDATE(TIM1);
        if(ftdi_latency_timer->callback) {
            ftdi_latency_timer->callback(ftdi_latency_timer->context);
        }
    }
}

static void ftdi_latency_timer_tim_init(FtdiLatencyTimer* ftdi_latency_timer) {
    furi_hal_bus_enable(FuriHalBusTIM1);

    LL_TIM_SetCounterMode(TIM1, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(TIM1, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetAutoReload(TIM1, 64000 - 1);
    LL_TIM_SetPrescaler(TIM1, FTDI_LATENCY_TIMER_SPEED_DEFALUT - 1);
    LL_TIM_SetClockSource(TIM1, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM1);

    furi_hal_interrupt_set_isr(
        FuriHalInterruptIdTim1UpTim16, ftdi_latency_timer_tim_isr, ftdi_latency_timer);

    LL_TIM_EnableIT_UPDATE(TIM1);
}

static void ftdi_latency_timer_tim_deinit(FtdiLatencyTimer* ftdi_latency_timer) {
    UNUSED(ftdi_latency_timer);
    LL_TIM_DisableCounter(TIM1);
    furi_hal_bus_disable(FuriHalBusTIM1);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTim1UpTim16, NULL, NULL);
}