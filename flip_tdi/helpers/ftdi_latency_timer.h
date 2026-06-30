#pragma once
#include "ftdi.h"

typedef struct FtdiLatencyTimer FtdiLatencyTimer;

typedef void (*FtdiLatencyTimerCallbackUp)(void* context);

FtdiLatencyTimer* ftdi_latency_timer_alloc(void);
void ftdi_latency_timer_free(FtdiLatencyTimer* ftdi_latency_timer);
void ftdi_latency_timer_set_callback(
    FtdiLatencyTimer* ftdi_latency_timer,
    FtdiLatencyTimerCallbackUp callback,
    void* context);
void ftdi_latency_timer_enable(FtdiLatencyTimer* ftdi_latency_timer, bool enable);
void ftdi_latency_timer_reset(FtdiLatencyTimer* ftdi_latency_timer);
void ftdi_latency_timer_set_speed(FtdiLatencyTimer* ftdi_latency_timer, uint8_t speed);
uint8_t ftdi_latency_timer_get_speed(FtdiLatencyTimer* ftdi_latency_timer);
