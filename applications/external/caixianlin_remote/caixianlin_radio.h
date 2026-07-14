#ifndef CAIXIANLIN_RADIO_H
#define CAIXIANLIN_RADIO_H

#include "caixianlin_types.h"

// Initialize radio hardware
bool caixianlin_radio_init(CaixianlinRemoteApp* app);

// Cleanup radio hardware
void caixianlin_radio_deinit(CaixianlinRemoteApp* app);

// Start transmission
void caixianlin_radio_start_tx(CaixianlinRemoteApp* app);

// Stop transmission
void caixianlin_radio_stop_tx(CaixianlinRemoteApp* app);

// Start listening for signals
void caixianlin_radio_start_rx(CaixianlinRemoteApp* app);

// Stop listening
void caixianlin_radio_stop_rx(CaixianlinRemoteApp* app);

// RX capture callback (internal, used by radio)
void caixianlin_radio_rx_callback(bool level, uint32_t duration, void* context);

#endif // CAIXIANLIN_RADIO_H
