#pragma once

#include "types.h"

/* Detect PWM encoding parameters by aggregating runs across the inner bits
 * of every segment (concatenated virtually, matching analyze.py behaviour).
 * Result placed in `out`. `out->found` is false if no PWM pattern found. */
void decoders_detect_pwm(const SubFile* sub, PWMParams* out);

/* Decode PWM-encoded bits using the given parameters. Writes up to `out_cap`
 * decoded bits into `out`. Returns the count actually written. */
uint16_t decoders_decode_pwm(
    const uint8_t* bits,
    uint16_t len,
    const PWMParams* pwm,
    uint8_t* out,
    uint16_t out_cap);

/* Decode Manchester-encoded bits, trying both conventions and returning the
 * lower-error one. Writes up to `out_cap` decoded bits into `out`. Returns
 * the count actually written. */
uint16_t decoders_decode_manchester(
    const uint8_t* bits,
    uint16_t len,
    uint8_t* out,
    uint16_t out_cap,
    ManchesterConvention* out_conv,
    float* out_error_rate);

/* Detect rolling code by PWM-decoding every segment's inner bits then
 * comparing them position-by-position. Diff positions written up to
 * `diff_cap`; if more positions exist `*out_diff_truncated` is set true. */
void decoders_detect_rolling_code(
    const SubFile* sub,
    const PWMParams* pwm,
    bool* out_rolling,
    bool* out_fixed,
    uint16_t* out_diff_positions,
    uint16_t diff_cap,
    uint16_t* out_diff_count,
    bool* out_diff_truncated);

/* Detect tri-state PWM: dominant pulse + three distinct gap buckets where the
 * third bucket carries >=10% of zero-runs. Returns true and fills the symbol
 * count when found. */
bool decoders_detect_pwm3(const SubFile* sub, uint16_t* out_symbol_count);

/* Scan PWM-decoded payload for CRC-8 (poly 0x07 or 0x31) or CRC-16-CCITT
 * trailer. Sets *out_kind to 0=none, 1=CRC-8, 2=CRC-16-CCITT. Returns true
 * when a valid trailer is found. */
bool decoders_detect_crc(const uint8_t* decoded_bits, uint16_t len, uint8_t* out_kind);
