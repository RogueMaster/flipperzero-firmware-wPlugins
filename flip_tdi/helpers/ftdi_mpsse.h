#pragma once
#include "ftdi.h"

typedef struct FtdiMpsse FtdiMpsse;

typedef void (*FtdiMpsseCallbackImmediate)(void* context);
void ftdi_mpsse_gpio_set_callback(FtdiMpsse* ftdi_mpsse, FtdiMpsseCallbackImmediate callback, void* context);

FtdiMpsse* ftdi_mpsse_alloc(Ftdi* ftdi);
void ftdi_mpsse_free(FtdiMpsse* ftdi_mpsse);
void ftdi_mpsse_state_machine(FtdiMpsse* ftdi_mpsse);
