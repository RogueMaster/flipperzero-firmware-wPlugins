#pragma once

#include <stdbool.h>
#include <stdint.h>

/* I2S output on SAI1.
 * BCK = PB3 (SCK_B), LRCK = PA4 (FS_B), DIN = PC3 (SD_A).
 * Format: Philips I2S stereo.
 *   16-bit: 16-bit slots, BCLK = 32 * Fs
 *   24-bit: 24-bit data MSB-first in 32-bit slots, BCLK = 64 * Fs
 *
 * Clock sources:
 *   Internal:  PLLSAI1, no MCLK. Block B is clock master, block A is a
 *              synchronous slave sending data.
 *   MCLK out:  PLLSAI1, MCLK = 256 * Fs on PB14. Same block layout as
 *              Internal; block A's divider drives MCLK_A, locked to BCK.
 *   External:  SAI kernel clock from an oscillator on PB2 (SAI1_EXTCLK), which
 *              also feeds the DAC's MCLK. Exact rates.
 * In MCLK out and External modes PC1 drives the DAC speed pin (high above
 * 50 kHz, CS4398 M3) and PC0 drives its reset (high while streaming). */

typedef enum {
    I2sClockInternal,
    I2sClockMclkOut,
    I2sClockExt24576,
    I2sClockExt22579,
    I2sClockCount,
} I2sClockSource;

/** Takes over clocks: USB -> HSI48 + CRS, configures pins for the source.
 *  Call while USB is disconnected. */
void i2s_out_init(I2sClockSource source);

/** Releases SAI, restores clock tree and pins. Call while USB is disconnected. */
void i2s_out_deinit(void);

/** Switches clock source and its pins. Stops output first. */
void i2s_out_set_source(I2sClockSource source);

I2sClockSource i2s_out_get_source(void);

/** Short human-readable name of a clock source */
const char* i2s_out_source_name(I2sClockSource source);

/** True if the current clock source can produce this rate and width */
bool i2s_out_rate_supported(uint32_t rate, uint8_t bits);

/** Starts circular DMA from buf at the given rate. buf holds interleaved L/R
 *  samples: int16_t for 16-bit, int32_t (24-bit value in low bits) for 24-bit.
 *  Safe to call from USB interrupt. */
bool i2s_out_start(uint32_t rate, uint8_t bits, void* buf, uint32_t len_samples);

void i2s_out_stop(void);

bool i2s_out_is_running(void);

/** Current DMA read position in samples */
uint32_t i2s_out_get_read_pos(void);

/** Exact sample rate produced by the clock tree, in milli-Hz */
uint32_t i2s_out_get_actual_rate_mhz(void);

/** Bit clock in Hz */
uint32_t i2s_out_get_bclk(void);

/** Master clock in Hz, 0 when no MCLK is used */
uint32_t i2s_out_get_mclk(void);

/** SAI kernel clock in Hz */
uint32_t i2s_out_get_sai_clk(void);
