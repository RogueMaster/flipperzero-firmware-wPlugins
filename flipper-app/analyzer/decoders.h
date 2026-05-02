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
