#pragma once

#include <furi.h>
#include <stdbool.h>
#include "../views/spectrum_view.h"

/*
 * NRF24 (nRF24L01+) 2.4 GHz spectrum analyzer.
 *
 * A background worker drives the NRF24 over the Flipper's external SPI bus and
 * sweeps all 126 RF channels (2400-2525 MHz). On each channel it puts the radio
 * in RX, lets the PLL settle, and samples the Received Power Detector (RPD): a
 * hardware flag that latches when in-band carrier power exceeds ~-64 dBm. Hits
 * accumulate with decay into a per-channel activity level, published as a
 * SpectrumSnapshot for the analyzer view.
 *
 * Wiring (standard Flipper <-> nRF24L01 mapping):
 *   pin 2 (A7) MOSI   pin 3 (A6) MISO   pin 4 (A4) CSN
 *   pin 5 (B3) SCK    pin 6 (B2) CE     3V3 -> VCC   GND -> GND
 *
 * Read-only: the analyzer never transmits.
 */

typedef struct Nrf24Radio Nrf24Radio;

Nrf24Radio* nrf24_radio_alloc(void);
void nrf24_radio_free(Nrf24Radio* radio);

void nrf24_radio_start(Nrf24Radio* radio);
void nrf24_radio_stop(Nrf24Radio* radio);
bool nrf24_radio_is_running(Nrf24Radio* radio);

// Clear the accumulated activity / peak hold. Safe to call while running.
void nrf24_radio_reset(Nrf24Radio* radio);

// Copy the latest spectrum for drawing.
void nrf24_radio_get_snapshot(Nrf24Radio* radio, SpectrumSnapshot* out);
