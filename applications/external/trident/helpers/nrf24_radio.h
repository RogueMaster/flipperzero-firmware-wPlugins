#pragma once

#include <furi.h>
#include <stdbool.h>
#include "../views/spectrum_view.h"
#include "../views/meter_view.h"

/*
 * NRF24 (nRF24L01+) 2.4 GHz radio: spectrum analyzer, channel finder and an
 * experimental promiscuous sniffer. A background worker drives the NRF24 over
 * the Flipper's external SPI bus in one of three modes:
 *   - Sweep : hop all 126 channels, sample the Received Power Detector, publish
 *             a SpectrumSnapshot (2.4 GHz spectrum).
 *   - Camp  : hold one channel, measure the RPD hit-rate, publish a
 *             MeterSnapshot (channel finder).
 *   - Sniff : promiscuous capture on one channel; each frame is handed to a
 *             line callback (address-less, CRC off - best effort).
 *
 * Wiring (standard Flipper <-> nRF24L01 mapping):
 *   pin 2 (A7) MOSI   pin 3 (A6) MISO   pin 4 (A4) CSN
 *   pin 5 (B3) SCK    pin 6 (B2) CE     3V3 -> VCC   GND -> GND
 *
 * Read-only: it never transmits.
 */

typedef enum {
    Nrf24ModeSweep = 0,
    Nrf24ModeCamp = 1,
    Nrf24ModeSniff = 2,
} Nrf24Mode;

typedef void (*Nrf24LineCallback)(void* context, const char* line);

typedef struct Nrf24Radio Nrf24Radio;

Nrf24Radio* nrf24_radio_alloc(void);
void nrf24_radio_free(Nrf24Radio* radio);

void nrf24_radio_set_mode(Nrf24Radio* radio, Nrf24Mode mode); // latched at start
void nrf24_radio_set_channel(Nrf24Radio* radio, uint8_t ch); // camp / sniff, live
uint8_t nrf24_radio_get_channel(Nrf24Radio* radio);
void nrf24_radio_set_line_callback(Nrf24Radio* radio, Nrf24LineCallback cb, void* context);

void nrf24_radio_start(Nrf24Radio* radio);
void nrf24_radio_stop(Nrf24Radio* radio);
bool nrf24_radio_is_running(Nrf24Radio* radio);

void nrf24_radio_reset(Nrf24Radio* radio); // clear activity / peak hold

void nrf24_radio_get_snapshot(Nrf24Radio* radio, SpectrumSnapshot* out); // sweep
void nrf24_radio_get_meter(Nrf24Radio* radio, MeterSnapshot* out); // camp / sniff
