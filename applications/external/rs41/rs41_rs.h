#pragma once
#include <stdint.h>

/*
 * RS41 Reed-Solomon error correction.
 *
 * RS(255,231) shortened to (156,132) per codeword.
 * Parameters: GF(2^8) poly=0x11D, FCR=0, α=2, NROOTS=24.
 *
 * Each RS41 frame (312 bytes, after bit-reverse + XOR descramble) carries:
 *   bytes[  0.. 23] — RS parity codeword 1
 *   bytes[ 24.. 47] — RS parity codeword 2
 *   bytes[ 48..311] — interleaved data (even offsets → CW1, odd → CW2)
 *
 * Returns: total errors corrected (both codewords), or -1 if uncorrectable.
 * The frame is corrected in-place.
 */
int rs41_rs_correct(uint8_t* frame, int frame_len);
