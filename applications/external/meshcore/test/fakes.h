/*
 * Test doubles for the two things the protocol layer depends on: the UART and
 * the debug log. Both are replaced wholesale — the real ones need furi_hal.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../meshcore_log.h"
#include "../uart/meshcore_uart.h"

/* ---- fake UART ---- */

/** Forget queued RX, recorded TX and the error counter. */
void fake_uart_reset(void);

/** Queue bytes for the link layer to read as if the node had sent them. */
void fake_uart_push_rx(const uint8_t* data, size_t len);

/** Everything the link layer has transmitted since the last reset. */
const uint8_t* fake_uart_tx_data(void);
size_t fake_uart_tx_len(void);

/** Pretend the line is noisy, so error diagnosis can be exercised. */
void fake_uart_set_rx_errors(uint32_t count);

/** How many times a read blocked with nothing to hand back. Used to prove a
 *  timeout actually waited rather than spinning. */
uint32_t fake_uart_starved_reads(void);

/* ---- fake log ---- */

void fake_log_reset(void);
MeshCoreLog* fake_log_instance(void);
uint32_t fake_log_frame_count(bool tx);
