// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>

#include <gblink/include/gblink.h>

struct gblink {
	const GpioPin *serin;
	const GpioPin *serout;
	const GpioPin *clk;
	const GpioPin *sd;
	gblink_mode mode;
	void (*callback)(void* cb_context, uint8_t in);
	void *cb_context;

	/* These two semaphores serve similar but distinct purposes. */
	/* The transfer semaphore is taken as soon as a transfer() request
	 * has been started. This is used in the function to wait until the
	 * transfer has been completed.
	 */
	FuriSemaphore *transfer_sem;
	/* The out byte semaphore is used to indicate that a byte transfer
	 * is in progress. This is used in the transfer function to not allow
	 * a transfer request if we're in the middle of sending a byte.
	 * The transfer semaphore is not used for that purpose since if the
	 * Flipper is in EXT clk mode, once a transfer() is started, there
	 * would be no way to both prevent transfer() from being called again
	 * as well as cancelling/changing what we're wanting to send. Using
	 * out byte semaphore means a transfer() can be called at any time,
	 * waited on synchronously for a timeout, and then re-called at a
	 * later time; while blocking that update if a byte is actually
	 * in the middle of being transmitted.
	 */
	FuriSemaphore *out_byte_sem;

	/* Used to lock out changing things after a certain point. Pinout,
	 * mode, etc.
	 * XXX: Might make more sense to use the mutex to protect a flag?
	 * Maybe a semaphore? Though I think that is the wrong use.
	 */
	FuriMutex *start_mutex;

	/*
	 * The following should probably have the world stopped around them
	 * if not modified in an interrupt context.
	 */
	uint8_t in;
	uint8_t out;
	uint8_t shift;
	uint8_t nobyte;

	/* Should only be changed when not in middle of tx, will affect a lot */
	gblink_clk_source source;

	/* Can be changed at any time, will only take effect on the next
	 * transfer.
	 */
	gblink_speed speed;


	/*
	 * The following is based on observing Pokemon trade data
	 *
	 * Clocks idle between bytes is nominally 430 us long for burst data,
	 * 15 ms for idle polling (e.g. waiting for menu selection), some oddball
	 * 2 ms gaps that appears between one 0xFE byte from the Game Boy every trade;
	 * clock period is nominally 122 us.
	 *
	 * Therefore, if we haven't seen a clock in 500 us, reset our bit counter.
	 * Note that, this should never actually be a concern, but it is an additional
	 * safeguard against desyncing.
	 */
	uint32_t time;
	uint32_t bitclk_timeout_us;

	void *exti_workaround_handle;
};
