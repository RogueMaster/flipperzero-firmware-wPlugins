/*
 * Purpose: Keep Morse Flipper USB identities consistent across HID and MIDI modes.
 * Owns: The shared vendor ID and mode-specific product IDs.
 * Depends on: Nothing.
 * Tests: tests/test_usb_ids.py.
 */

#pragma once

#define MORSE_USB_VID          0x1209U
#define MORSE_USB_KEYBOARD_PID 0x6900U
#define MORSE_USB_MOUSE_PID    0x6901U
#define MORSE_USB_MIDI_PID     0x6902U
