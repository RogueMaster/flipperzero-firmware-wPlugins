// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023 KBEmbedded

#ifndef __GBLINK_H__
#define __GBLINK_H__

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \enum gblink_clk_source
 * \brief Clock sources available
 */
typedef enum {
	//! Flipper generates the clock internally
	GBLINK_CLK_INT,
	//! Flipper expects a clock input
	GBLINK_CLK_EXT,
} gblink_clk_source;

/**
 * Currently unused
 */
typedef enum {
	GBLINK_MODE_GBC,
	GBLINK_MODE_GBA,
} gblink_mode;

/**
 * When using GBLINK_CLK_INT, the rate at which the Flipper drives the clock
 *
 * Specified speed does not matter if GBLINK_CLK_EXT.
 *
 * Anything above GBLINK_SPD_8192HZ only applies to GBC. The original DMG
 * and pocket variants of the Game Boy _can_ receive at higher rates, but
 * it may have issues.
 */
typedef enum {
	GBLINK_SPD_8192HZ = 4096,
	GBLINK_SPD_16384HZ = 8192,
	GBLINK_SPD_262144HZ = 16384,
	GBLINK_SPD_524288HZ = 262144,
} gblink_speed;

typedef enum {
	PINOUT_CUSTOM = -1,
	PINOUT_ORIGINAL = 0,
	PINOUT_MALVEKE_EXT1,
	PINOUT_COUNT,
} gblink_pinouts;

typedef enum {
	PIN_SERIN,
	PIN_SEROUT,
	PIN_CLK,
	PIN_SD,
	PIN_COUNT,
} gblink_bus_pins;

/**
 * Set the clock source for transfer, internal or external.
 *
 * @param handle Pointer to gblink handle
 * @param clk_source Specify Flipper expects an internal or external clock
 *
 * @note This can be called at any time, it will reset the current byte transfer
 * if called mid-transfer.
 */
void gblink_clk_source_set(void *handle, gblink_clk_source clk_source);

/**
 * Set the clock rate for GBLINK_CLK_INT transfer
 *
 * @param handle Pointer to gblink handle
 * @param speed Clock rate to be used
 *
 * @note This can be called at any time, changes will take effect on the next
 * byte transfer start.
 *
 * @note This can be arbitrary if really needed, the value passed needs to be
 * the desired frequency in Hz/2. e.g. passing 512 will result in a 1 kHz clock.
 */
void gblink_speed_set(void *handle, gblink_speed speed);

/**
 * Set up an inter-bit timeout
 *
 * Specify a timeout in microseconds that, when exceeded, will reset the transfer
 * state machine. If a transfer is interrupted or errant clocks are received when
 * connecting/disconnecting the Link Cable, this will ensure the state machine
 * gets reset with the start of the next clock.
 *
 * @note This can apply to GBLINK_CLK_INT, but only if there are significant
 * issues, e.g. the CPU is starved too long and the timer doesn't get called
 * in a reasonable amount of time.
 *
 * @note This defaults to 500 us if unset.
 *
 * @param handle Pointer to gblink handle
 * @param us Inter-bit timeout to set in microseconds.
 */
void gblink_timeout_set(void *handle, uint32_t us);

/**
 * Set up a data transfer
 *
 * When in GBLINK_CLK_INT, this initiates a transfer immediately.
 * When in GBLINK_CLK_EXT, this pre-loads data to transmit to the link partner.
 *
 * In both cases, the call is non-blocking and the transfer will happen
 * asynchronously. If a blocking call is needed, then gblink_transfer_tx_wait_complete()
 * should be called immediately after.
 *
 * @param handle Pointer to gblink handle
 * @param val 8-bit value to transmit
 *
 * @returns true if TX data was properly set up to transmit, false if there
 * was an error or a transfer is in progress already.
 */
bool gblink_transfer(void *handle, uint8_t val);

/**
 * Set a callback to call in to after each byte received
 *
 * @param handle Pointer to gblink handle
 * @param callback Pointer to callback function
 * @param cb_context Pointer to a context to pass to the callback
 *
 * @note The gblink instance must not be gblink_start()'ed!
 *
 * @note If no callback is set, then gblink_transfer_tx_wait_complete() must be
 * used after each call to gblink_transfer() to acquire the received data!
 *
 * @returns 0 on success, 1 if gblink instance is not gblink_stop()'ed.
 */
int gblink_callback_set(void *handle, void (*callback)(void* cb_context, uint8_t in), void *cb_context);

/**
 * Set the link interface mode
 *
 * @param handle Pointer to gblink handle
 * @param mode Mode of operation
 *
 * @note The gblink instance must not be gblink_start()'ed!
 *
 * @returns 0 on success, 1 if gblink instance is not gblink_stop()'ed.
 *
 * @deprecated Only GBLINK_MODE_GBC is used at this time.
 */
int gblink_mode_set(void *handle, gblink_mode mode);

/**
 * Wait for a transfer to complete
 *
 * This can be used for INT or EXT clock modes. After a call to gblink_transfer(),
 * this can be called at any time and will return only after a full byte is
 * transferred.
 *
 * @param handle Pointer to gblink handle
 *
 * @returns The last byte received from the link partner
 */
uint8_t gblink_transfer_tx_wait_complete(void *handle);

/**
 * Set a dummy byte to load in to TX buffer in case real data not available in time
 *
 * This is very specific to individual uses of the Link interface, but, some games
 * and tools have a byte that the EXT clock side can set in the TX immediately
 * after a byte transfer is complete. If the INT clock side sees this byte, then
 * it knows that the EXT clock side was not ready and can retry or do something
 * else.
 *
 * For example, Pokemon Gen I/II trade link interface uses 0xFE to tell the INT
 * clock side that data was not ready yet. The INT clock will continue to repeat
 * sending the same byte until the EXT clock side finally sends valid data.
 *
 * @note This is specific to what the link partner expects!
 */
void gblink_nobyte_set(void *handle, uint8_t val);

/**
 * Enable interrupts on gblink clock pin
 *
 * @param handle Pointer to gblink handle
 *
 * @deprecated This may go away. Use of gblink_start() and gblink_stop() are
 * preferred.
 */
void gblink_int_enable(void *handle);

/**
 * Disable interrupts on gblink clock pin
 *
 * @param handle Pointer to gblink handle
 *
 * @deprecated This may go away. Use of gblink_start() and gblink_stop() are
 * preferred.
 */
void gblink_int_disable(void *handle);

/**
 * Allocate a handle of a gblink instance
 *
 * @returns pointer to handle
 */
void *gblink_alloc(void);

/**
 * Free a gblink instance
 *
 * @param handle Pointer to gblink handle
 */
void gblink_free(void *handle);

/**
 * Start a gblink instance
 *
 * This will enable interrupts if EXT clock, as well as prevents some changes
 * being made. e.g. pin assignments, mode, etc.
 *
 * @param handle Pointer to gblink handle
 */
void gblink_start(void *handle);

/**
 * Stop a gblink instance
 *
 * Disables interrupts, stops any pending timers, and enters back to an idle
 * state. Once called, re-allows changes to be made.
 *
 * @param handle Pointer to gblink handle
 */
void gblink_stop(void *handle);

// void gblink_blink_led_on_byte(handle, color?)
// get blink?

#ifdef __cplusplus
}
#endif

#endif // __GBLINK_H__
