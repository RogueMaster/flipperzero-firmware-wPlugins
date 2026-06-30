#pragma GCC optimize("O3")
#pragma GCC optimize("-funroll-all-loops")

#include "ftdi_bitbang.h"
#include "furi.h"
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>
#include "ftdi_gpio.h"

#define TAG "FTDI_BITBANG"

typedef enum {
    FtdiBitbangModeOff = (0UL),
    FtdiBitbangModeBitbang = (1UL),
    FtdiBitbangModeSyncbb = (2UL),
    FtdiBitbangModeMpsse = (3UL),
    FtdiBitbangModeReserved = (4UL),
} FtdiBitbangMode;

typedef void (*FtdiBitbangGpioIO)(uint8_t state);
struct FtdiBitbang {
    FuriThread* worker_thread;
    Ftdi* ftdi;
    FtdiMpsse* ftdi_mpsse;
    uint32_t speed;
    FtdiBitbangMode mode;

    uint8_t gpio_mask;
    FtdiBitbangGpioIO gpio_o[8];
    uint8_t gpio_data;
};

typedef enum {
    WorkerEventReserved = (1 << 0),
    WorkerEventStop = (1 << 1),
    WorkerEventTimerUpdate = (1 << 2),
} WorkerEvent;

#define WORKER_EVENTS_MASK (WorkerEventStop | WorkerEventTimerUpdate)

static void ftdi_bitbang_tim_init(FtdiBitbang* ftdi_bitbang);
static void ftdi_bitbang_tim_deinit(FtdiBitbang* ftdi_bitbang);

void ftdi_bitbang_gpio_set_direction(FtdiBitbang* ftdi_bitbang) {
    ftdi_gpio_set_direction(ftdi_bitbang->gpio_mask);
    if(ftdi_bitbang->gpio_mask & 0b00000001) {
        ftdi_bitbang->gpio_o[0] = ftdi_gpio_set_b0;
    } else {
        ftdi_bitbang->gpio_o[0] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b00000010) {
        ftdi_bitbang->gpio_o[1] = ftdi_gpio_set_b1;
    } else {
        ftdi_bitbang->gpio_o[1] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b00000100) {
        ftdi_bitbang->gpio_o[2] = ftdi_gpio_set_b2;
    } else {
        ftdi_bitbang->gpio_o[2] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b00001000) {
        ftdi_bitbang->gpio_o[3] = ftdi_gpio_set_b3;
    } else {
        ftdi_bitbang->gpio_o[3] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b00010000) {
        ftdi_bitbang->gpio_o[4] = ftdi_gpio_set_b4;
    } else {
        ftdi_bitbang->gpio_o[4] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b00100000) {
        ftdi_bitbang->gpio_o[5] = ftdi_gpio_set_b5;
    } else {
        ftdi_bitbang->gpio_o[5] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b01000000) {
        ftdi_bitbang->gpio_o[6] = ftdi_gpio_set_b6;
    } else {
        ftdi_bitbang->gpio_o[6] = ftdi_gpio_set_noop;
    }

    if(ftdi_bitbang->gpio_mask & 0b10000000) {
        ftdi_bitbang->gpio_o[7] = ftdi_gpio_set_b7;
    } else {
        ftdi_bitbang->gpio_o[7] = ftdi_gpio_set_noop;
    }
}

void ftdi_bitbang_gpio_init(FtdiBitbang* ftdi_bitbang) {
    ftdi_gpio_init(ftdi_bitbang->gpio_mask);
    ftdi_bitbang_gpio_set_direction(ftdi_bitbang);
}

static inline void ftdi_bitbang_gpio_set(FtdiBitbang* ftdi_bitbang, uint8_t gpio_data_out) {
    ftdi_bitbang->gpio_o[0](gpio_data_out & 0b00000001);
    ftdi_bitbang->gpio_o[1](gpio_data_out & 0b00000010);
    ftdi_bitbang->gpio_o[2](gpio_data_out & 0b00000100);
    ftdi_bitbang->gpio_o[3](gpio_data_out & 0b00001000);
    ftdi_bitbang->gpio_o[4](gpio_data_out & 0b00010000);
    ftdi_bitbang->gpio_o[5](gpio_data_out & 0b00100000);
    ftdi_bitbang->gpio_o[6](gpio_data_out & 0b01000000);
    ftdi_bitbang->gpio_o[7](gpio_data_out & 0b10000000);
}

static inline uint8_t ftdi_bitbang_gpio_get(void) {
    uint8_t gpio_data = 0;
    gpio_data |= ftdi_gpio_get_b0();
    gpio_data |= ftdi_gpio_get_b1();
    gpio_data |= ftdi_gpio_get_b2();
    gpio_data |= ftdi_gpio_get_b3();
    gpio_data |= ftdi_gpio_get_b4();
    gpio_data |= ftdi_gpio_get_b5();
    gpio_data |= ftdi_gpio_get_b6();
    gpio_data |= ftdi_gpio_get_b7();
    return gpio_data;
}

static int32_t ftdi_bitbang_worker(void* context) {
    furi_assert(context);
    FtdiBitbang* ftdi_bitbang = context;

    uint8_t buffer[64];

    FURI_LOG_I(TAG, "Worker started");
    ftdi_bitbang_tim_init(ftdi_bitbang);
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_EVENTS_MASK, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEventTimerUpdate) {
            if(ftdi_bitbang->mode == FtdiBitbangModeMpsse) {
                ftdi_mpsse_state_machine(ftdi_bitbang->ftdi_mpsse);
            } else {
                size_t length = ftdi_get_rx_buf(ftdi_bitbang->ftdi, buffer, 1, 0);
                if(length > 0) {
                    ftdi_bitbang_gpio_set(ftdi_bitbang, buffer[0]);
                    if(ftdi_bitbang->mode == FtdiBitbangModeSyncbb) {
                        ftdi_bitbang->gpio_data = ftdi_bitbang_gpio_get();
                        ftdi_set_tx_buf(ftdi_bitbang->ftdi, &ftdi_bitbang->gpio_data, 1);
                    }
                }
                if(ftdi_bitbang->mode == FtdiBitbangModeBitbang) {
                    ftdi_bitbang->gpio_data = ftdi_bitbang_gpio_get();
                    ftdi_set_tx_buf(ftdi_bitbang->ftdi, &ftdi_bitbang->gpio_data, 1);
                }
            }
            continue;
        }

        if(events & WorkerEventStop) break;
    }
    ftdi_bitbang_tim_deinit(ftdi_bitbang);

    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

uint8_t ftdi_bitbang_get_gpio(FtdiBitbang* ftdi_bitbang) {
    return ftdi_bitbang->gpio_data = ftdi_bitbang_gpio_get();
}

FtdiBitbang* ftdi_bitbang_alloc(Ftdi* ftdi) {
    FtdiBitbang* ftdi_bitbang = malloc(sizeof(FtdiBitbang));
    ftdi_bitbang->ftdi = ftdi;
    ftdi_bitbang->ftdi_mpsse = ftdi_mpsse_alloc(ftdi);
    ftdi_bitbang->mode = FtdiBitbangModeOff;
    ftdi_bitbang->speed = 10000;
    ftdi_bitbang->gpio_mask = 0;
    ftdi_bitbang_gpio_init(ftdi_bitbang);
    ftdi_bitbang->worker_thread =
        furi_thread_alloc_ex(TAG, 1024, ftdi_bitbang_worker, ftdi_bitbang);

    furi_thread_start(ftdi_bitbang->worker_thread);
    return ftdi_bitbang;
}

void ftdi_bitbang_free(FtdiBitbang* ftdi_bitbang) {
    if(!ftdi_bitbang) return;

    ftdi_bitbang->mode = FtdiBitbangModeOff;
    ftdi_mpsse_free(ftdi_bitbang->ftdi_mpsse);
    furi_thread_flags_set(ftdi_bitbang->worker_thread, WorkerEventStop);
    furi_thread_join(ftdi_bitbang->worker_thread);
    furi_thread_free(ftdi_bitbang->worker_thread);

    ftdi_gpio_deinit();

    free(ftdi_bitbang);
    ftdi_bitbang = NULL;
}

FtdiMpsse* ftdi_bitbang_get_mpsse_handle(FtdiBitbang* ftdi_bitbang) {
    return ftdi_bitbang->ftdi_mpsse;
}

void ftdi_bitbang_set_gpio(FtdiBitbang* ftdi_bitbang, uint8_t gpio_mask) {
    ftdi_bitbang->gpio_mask = gpio_mask;
    ftdi_bitbang_gpio_set_direction(ftdi_bitbang);
}

void ftdi_bitbang_enable(FtdiBitbang* ftdi_bitbang, FtdiBitMode mode) {
    if(mode.BITBANG) {
        ftdi_bitbang->mode = FtdiBitbangModeBitbang;
    } else if(mode.SYNCBB) {
        ftdi_bitbang->mode = FtdiBitbangModeSyncbb;
    } else if(mode.MPSSE) {
        ftdi_bitbang->mode = FtdiBitbangModeMpsse;
    } else {
        ftdi_bitbang->mode = FtdiBitbangModeOff;
    }

    if(ftdi_bitbang->mode != FtdiBitbangModeOff) {
        LL_TIM_SetCounter(TIM17, 0);
        LL_TIM_EnableCounter(TIM17);
    } else {
        LL_TIM_DisableCounter(TIM17);
    }
}

void ftdi_bitbang_set_speed(FtdiBitbang* ftdi_bitbang, uint32_t speed) {
    UNUSED(ftdi_bitbang);
    UNUSED(speed);

    //Todo limit speed
    if(speed > 40000) {
        speed = 40000;
    }

    uint32_t freq_div = 64000000LU / speed;
    uint32_t prescaler = freq_div / 0x10000LU;
    uint32_t period = freq_div / (prescaler + 1);
    LL_TIM_SetPrescaler(TIM17, prescaler);
    LL_TIM_SetAutoReload(TIM17, period - 1);
}

static void ftdi_bitbang_tim_isr(void* context) {
    FtdiBitbang* ftdi_bitbang = context;
    UNUSED(ftdi_bitbang);
    if(LL_TIM_IsActiveFlag_UPDATE(TIM17)) {
        LL_TIM_ClearFlag_UPDATE(TIM17);
        //Speed max 40kHz
        furi_thread_flags_set(
            furi_thread_get_id(ftdi_bitbang->worker_thread), WorkerEventTimerUpdate);
        //Todo Speed Max 70kHz
        // uint8_t buffer[1];
        // size_t length = ftdi_get_rx_buf(ftdi_bitbang->ftdi, buffer, 1);
        // if(length > 0) {
        //     ftdi_bitbang_gpio_set(ftdi_bitbang, buffer[0]);
        //     if(!ftdi_bitbang->async) {
        //         ftdi_bitbang->gpio_data = ftdi_bitbang_gpio_get();
        //         ftdi_set_tx_buf(ftdi_bitbang->ftdi, &ftdi_bitbang->gpio_data, 1);
        //     }
        // }
        // if(ftdi_bitbang->enable && ftdi_bitbang->async) {
        //     ftdi_bitbang->gpio_data = ftdi_bitbang_gpio_get();
        //     ftdi_set_tx_buf(ftdi_bitbang->ftdi, &ftdi_bitbang->gpio_data, 1);
        // }
    }
}

static void ftdi_bitbang_tim_init(FtdiBitbang* ftdi_bitbang) {
    furi_hal_bus_enable(FuriHalBusTIM17);

    LL_TIM_SetCounterMode(TIM17, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(TIM17, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetAutoReload(TIM17, 6400 - 1);
    LL_TIM_SetPrescaler(TIM17, 0); // 10kHz
    LL_TIM_SetClockSource(TIM17, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_DisableARRPreload(TIM17);

    furi_hal_interrupt_set_isr(
        FuriHalInterruptIdTim1TrgComTim17, ftdi_bitbang_tim_isr, ftdi_bitbang);

    LL_TIM_EnableIT_UPDATE(TIM17);
}

static void ftdi_bitbang_tim_deinit(FtdiBitbang* ftdi_bitbang) {
    UNUSED(ftdi_bitbang);
    LL_TIM_DisableCounter(TIM17);
    furi_hal_bus_disable(FuriHalBusTIM17);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTim1TrgComTim17, NULL, NULL);
}