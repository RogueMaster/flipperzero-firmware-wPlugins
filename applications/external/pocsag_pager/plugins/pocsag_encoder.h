#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef POCSAG_ENCODER_HOST_TEST
#include <toolbox/level_duration.h>
#endif
#include "../helpers/pocsag_pager_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POCSAG_RIC_MAX         PCSG_RIC_MAX
#define POCSAG_MAX_MSG_LEN     PCSG_MAX_MSG_LEN
#define POCSAG_MAX_CODEWORDS   128U
#define POCSAG_PREAMBLE_BITS   576U
#define POCSAG_FRAME_SYNC_WORD 0x7CD215D8U
#define POCSAG_IDLE_WORD       0x7A89C197U

typedef struct {
    uint32_t codewords[POCSAG_MAX_CODEWORDS];
    size_t codeword_count;
    uint32_t baud;
    bool inverted;
    uint32_t preamble_bits;
    uint32_t total_bits;
    volatile uint32_t bit_idx;
    bool last_level;
    bool done;
} PocsagEncoder;

/** Compute BCH(31,21) check bits and even parity for a codeword whose 21 data bits are in bits [31:11]. */
uint32_t pocsag_bch_encode(uint32_t data_top21_shifted);

/**
 * Encode a POCSAG transmission into 32-bit codewords (sync + batch frames + idle padding).
 * Returns the number of codewords written, or 0 on error.
 */
size_t pocsag_encode_codewords(
    uint32_t ric,
    PocsagMsgType msg_type,
    const char* message,
    uint32_t* out_codewords,
    size_t max_codewords);

/** Prepare a streaming POCSAG encoder for async TX. Returns false on invalid parameters. */
bool pocsag_encoder_init(
    PocsagEncoder* enc,
    uint32_t ric,
    PocsagMsgType msg_type,
    const char* message,
    PocsagBaudRate baud,
    bool inverted);

/** Total air duration of the prepared transmission in milliseconds. */
uint32_t pocsag_encoder_get_duration_ms(const PocsagEncoder* enc);

/** Compute the next run-length pulse (level, duration_us). Returns false when finished. */
bool pocsag_encoder_next_pulse(PocsagEncoder* enc, bool* out_level, uint32_t* out_duration_us);

#ifndef POCSAG_ENCODER_HOST_TEST
/** FuriHalSubGhzAsyncTxCallback-compatible yield function for subghz_devices_start_async_tx. */
LevelDuration pocsag_encoder_yield(void* context);
#endif

#ifdef __cplusplus
}
#endif
