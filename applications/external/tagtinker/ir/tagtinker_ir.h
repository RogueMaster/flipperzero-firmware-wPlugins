/*
 * IR transmitter API.
 *
 * Sends already-built ESL frames over the built-in IR LED.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void tagtinker_ir_init(void);

void tagtinker_ir_deinit(void);

/* Repeats are sent with PP4 timing; the high bit is ignored for legacy saved data. */
bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats, uint8_t delay);

bool tagtinker_ir_is_busy(void);

void tagtinker_ir_stop(void);
