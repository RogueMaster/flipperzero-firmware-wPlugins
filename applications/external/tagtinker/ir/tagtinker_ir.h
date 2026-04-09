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

/* The high bit of repeats selects PP16. The low 15 bits are the repeat count. */
bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats, uint8_t delay);

bool tagtinker_ir_is_busy(void);

void tagtinker_ir_stop(void);
