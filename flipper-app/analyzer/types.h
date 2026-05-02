#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BITRAW_MAX_SEGMENTS         16
#define BITRAW_MAX_BITS_PER_SEG     8192
#define BITRAW_MAX_TOTAL_BITS       16384
#define BITRAW_MAX_DECODED_BITS     256
#define BITRAW_MAX_MANCH_BITS       4096
#define BITRAW_MAX_DIFF_POSITIONS   64
#define BITRAW_MAX_REASONS          16
#define BITRAW_MAX_REASON_LEN       128
#define BITRAW_MAX_HINTS            6
#define BITRAW_MAX_HINT_LEN         96
#define BITRAW_MAX_WARNINGS         6
#define BITRAW_MAX_WARNING_LEN      128
#define BITRAW_MAX_PRESET_LEN       64
#define BITRAW_MAX_PATH_LEN         256

typedef struct {
    char preset[BITRAW_MAX_PRESET_LEN];
    uint32_t frequency_hz;
    uint32_t te_us;
    uint32_t total_bit_header;
    float lat;
    float lon;
    bool has_gps;

    uint16_t segment_count;
    uint16_t segment_bit_lens[BITRAW_MAX_SEGMENTS];
    uint8_t* segment_bits[BITRAW_MAX_SEGMENTS];

    /* Inner-bit ranges (after strip_padding); inner_len == 0 means empty. */
    uint16_t inner_start[BITRAW_MAX_SEGMENTS];
    uint16_t inner_len[BITRAW_MAX_SEGMENTS];

    bool truncated;
} SubFile;

typedef struct {
    bool found;
    uint16_t pulse_width;
    uint16_t short_gap;
    uint16_t long_gap;
    float consistency;
} PWMParams;

typedef struct {
    bool found;
    uint16_t length;
    uint16_t position;
} PreambleInfo;

typedef enum {
    ManchesterGEThomas = 0,
    ManchesterIEEE8023 = 1,
} ManchesterConvention;

typedef struct {
    /* From SubFile */
    uint32_t frequency;
    float te_us;
    float bitrate_bps;

    uint16_t seg_count;
    uint16_t seg_sizes[BITRAW_MAX_SEGMENTS];
    uint32_t total_bits;

    uint16_t inner_sizes[BITRAW_MAX_SEGMENTS];
    uint32_t total_inner_bits;
    uint32_t inner_set_bits; /* count of 1s in inner bits (used by NOISE rules) */
    float mean_inner_size;

    float zero_ratio;
    float entropy;

    uint16_t dominant_1run;
    uint16_t dominant_0run;
    float run_variety;

    PWMParams pwm_params;

    uint8_t pwm_decoded_bits[BITRAW_MAX_DECODED_BITS];
    uint16_t pwm_decoded_count;

    PreambleInfo preamble;

    bool has_seg_similarity;
    float seg_similarity;

    uint16_t repeating_subpattern_period; /* 0 = none */
    uint16_t repeating_subpattern_reps;

    uint8_t manchester_decoded_bits[BITRAW_MAX_MANCH_BITS];
    uint16_t manchester_decoded_count;
    float manchester_error_rate;
    ManchesterConvention manchester_convention;

    bool rolling_code;
    bool fixed_code;
    uint16_t diff_positions[BITRAW_MAX_DIFF_POSITIONS];
    uint16_t diff_position_count;
    bool diff_positions_truncated; /* >BITRAW_MAX_DIFF_POSITIONS positions exist */

    float signal_quality;

    float lat;
    float lon;
    bool has_gps;
} FeatureVector;

typedef enum {
    BitrawLabelNoise = 0,
    BitrawLabelAmrMeter,
    BitrawLabelTpms,
    BitrawLabelAlarmSensor,
    BitrawLabelShutterBlind,
    BitrawLabelDoorbell,
    BitrawLabelOutletSwitch,
    BitrawLabelGarageRemote,
    BitrawLabelKeyfobRemote,
    BitrawLabelWeatherStation,
    BitrawLabelUnknownStructured,
} BitrawLabel;

typedef enum {
    BitrawConfLow = 0,
    BitrawConfMedium,
    BitrawConfHigh,
} BitrawConfidence;

typedef struct {
    BitrawLabel label;
    BitrawConfidence confidence;

    char hints[BITRAW_MAX_HINTS][BITRAW_MAX_HINT_LEN];
    uint8_t hint_count;

    char reasons[BITRAW_MAX_REASONS][BITRAW_MAX_REASON_LEN];
    uint8_t reason_count;

    char warnings[BITRAW_MAX_WARNINGS][BITRAW_MAX_WARNING_LEN];
    uint8_t warning_count;
} ClassificationResult;

const char* bitraw_label_name(BitrawLabel label);
const char* bitraw_confidence_name(BitrawConfidence c);
const char* bitraw_manchester_name(ManchesterConvention c);
