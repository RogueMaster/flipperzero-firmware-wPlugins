#pragma once
#include "ftdi.h"

typedef struct FtdiUart FtdiUart;

FtdiUart* ftdi_uart_alloc(Ftdi* ftdi);
void ftdi_uart_free(FtdiUart* ftdi_uart);
void ftdi_uart_tx(FtdiUart* ftdi_uart);
void ftdi_uart_set_baudrate(FtdiUart* ftdi_uart, uint32_t baudrate);
void ftdi_uart_set_data_config(FtdiUart* ftdi_uart, FtdiDataConfig* data_config);
void ftdi_uart_enable(FtdiUart* ftdi_uart, bool enable);