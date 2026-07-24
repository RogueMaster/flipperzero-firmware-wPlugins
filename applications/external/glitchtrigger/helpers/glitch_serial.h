#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Watches the target's UART (USART, pins 13 TX / 14 RX) for a success string.
 * A sweep can use this instead of a GPIO feedback pin: if the substring appears
 * on the serial line after a shot, that shot glitched.
 *
 * RX runs in interrupt context into a stream buffer; call glitch_serial_poll()
 * from the UI thread to drain it through an (always-correct sliding-window)
 * substring matcher. */

typedef struct GlitchSerial GlitchSerial;

GlitchSerial* glitch_serial_alloc(void);
void glitch_serial_free(GlitchSerial* s);

/* Acquire the USART at `baud` and start watching for `needle`. Returns false if
 * the port is busy or the needle is empty. */
bool glitch_serial_start(GlitchSerial* s, uint32_t baud, const char* needle);
void glitch_serial_stop(GlitchSerial* s);
bool glitch_serial_running(const GlitchSerial* s);

/* Drain received bytes through the matcher (UI thread). */
void glitch_serial_poll(GlitchSerial* s);
/* True if the needle has been seen since the last clear. */
bool glitch_serial_matched(const GlitchSerial* s);
/* Reset the "seen" flag (call after evaluating each sweep point). */
void glitch_serial_clear(GlitchSerial* s);
