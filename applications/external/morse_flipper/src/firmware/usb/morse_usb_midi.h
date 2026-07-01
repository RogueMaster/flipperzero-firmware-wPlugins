/*
 * Purpose: Publish the USB MIDI interface and packet I/O API.
 * Owns: MIDI USB interface declaration, RX callback hooks, and write helpers.
 * Depends on: Flipper USB HAL.
 * Tests: firmware build; USB behaviour is hardware-only.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <furi_hal_usb.h>

extern FuriHalUsbInterface morse_usb_midi_interface;

typedef void (*MorseUsbMidiRxCallback)(void* context);

void morse_usb_midi_set_context(void* context);
void morse_usb_midi_set_rx_callback(MorseUsbMidiRxCallback callback);

bool morse_usb_midi_is_connected(void);
size_t morse_usb_midi_rx(uint8_t* buffer, size_t size);
size_t morse_usb_midi_tx(const uint8_t* buffer, size_t size);
