#pragma once
#include "ftdi_usb_define.h"

//#define FTDI_DEBUG

typedef struct Ftdi Ftdi;

typedef void (*FtdiCallbackTxImmediate)(void* context);

Ftdi* ftdi_alloc(void);
void ftdi_free(Ftdi* ftdi);
void ftdi_set_callback_tx_immediate(Ftdi* ftdi, FtdiCallbackTxImmediate callback, void* context);
void ftdi_reset_purge_rx(Ftdi* ftdi);
void ftdi_reset_purge_tx(Ftdi* ftdi);
void ftdi_reset_sio(Ftdi* ftdi);
uint32_t ftdi_set_tx_buf(Ftdi* ftdi, const uint8_t* data, uint32_t size);
uint32_t ftdi_get_tx_buf(Ftdi* ftdi, uint8_t* data, uint32_t size);
uint32_t ftdi_available_tx_buf(Ftdi* ftdi);
uint32_t ftdi_set_rx_buf(Ftdi* ftdi, const uint8_t* data, uint32_t size);
uint32_t ftdi_get_rx_buf(Ftdi* ftdi, uint8_t* data, uint32_t size, uint32_t timeout);
uint32_t ftdi_available_rx_buf(Ftdi* ftdi);
void ftdi_loopback(Ftdi* ftdi);
void ftdi_set_baudrate(Ftdi* ftdi, uint16_t value, uint16_t index);
void ftdi_set_data_config(Ftdi* ftdi, uint16_t value, uint16_t index);
void ftdi_set_flow_ctrl(Ftdi* ftdi, uint16_t index);
void ftdi_set_bitmode(Ftdi* ftdi, uint16_t value, uint16_t index);
void ftdi_set_latency_timer(Ftdi* ftdi, uint16_t value, uint16_t index);
uint8_t ftdi_get_latency_timer(Ftdi* ftdi);
void ftdi_reset_latency_timer(Ftdi* ftdi);
uint16_t* ftdi_get_modem_status_uint16_t(Ftdi* ftdi);
FtdiModemStatus ftdi_get_modem_status(Ftdi* ftdi);
void ftdi_set_modem_status(Ftdi* ftdi, FtdiModemStatus status);

void ftdi_start_uart_tx(Ftdi* ftdi);
uint8_t ftdi_get_bitbang_gpio(Ftdi* ftdi);
