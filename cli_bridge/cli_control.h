#pragma once

#include <furi.h>
#include <furi_hal.h>
extern void clicontrol_hijack(size_t tx_size, size_t rx_size);
extern void clicontrol_unhijack(bool persist);
extern FuriStreamBuffer* cli_tx_stream;
extern FuriStreamBuffer* cli_rx_stream;
