/* Host stand-in for furi_hal.
 *
 * Randomness is replayable: a test queues the octets a session should draw so a
 * recorded exchange reproduces exactly. With nothing queued it returns a
 * counting ramp, so an unscripted session is still deterministic.
 */
#pragma once

#include <furi.h>

void furi_hal_random_fill_buf(uint8_t* buf, uint32_t len);

/* Queue octets for the next furi_hal_random_fill_buf calls. */
void seos_host_set_random(const uint8_t* data, size_t len);
void seos_host_clear_random(void);
size_t seos_host_random_remaining(void);
