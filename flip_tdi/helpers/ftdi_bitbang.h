#pragma once
#include "ftdi.h"
#include "ftdi_mpsse.h"

typedef struct FtdiBitbang FtdiBitbang;

FtdiBitbang* ftdi_bitbang_alloc(Ftdi* ftdi);
void ftdi_bitbang_free(FtdiBitbang* ftdi_bitbang);
FtdiMpsse* ftdi_bitbang_get_mpsse_handle(FtdiBitbang* ftdi_bitbang);
void ftdi_bitbang_set_gpio(FtdiBitbang* ftdi_bitbang, uint8_t gpio_mask);
void ftdi_bitbang_enable(FtdiBitbang* ftdi_bitbang, FtdiBitMode mode);
void ftdi_bitbang_set_speed(FtdiBitbang* ftdi_bitbang, uint32_t speed);
uint8_t ftdi_bitbang_get_gpio(FtdiBitbang* ftdi_bitbang);