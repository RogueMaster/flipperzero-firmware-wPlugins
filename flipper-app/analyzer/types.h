#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUBHOUND_MAX_SEGMENTS         16
#define SUBHOUND_MAX_BITS_PER_SEG     8192
#define SUBHOUND_MAX_TOTAL_BITS       16384
#define SUBHOUND_MAX_DECODED_BITS     256
#define SUBHOUND_MAX_MANCH_BITS       4096
#define SUBHOUND_MAX_DIFF_POSITIONS   64
#define SUBHOUND_MAX_REASONS          16
#define SUBHOUND_MAX_REASON_LEN       128
#define SUBHOUND_MAX_HINTS            6
#define SUBHOUND_MAX_HINT_LEN         96
#define SUBHOUND_MAX_WARNINGS         6
#define SUBHOUND_MAX_WARNING_LEN      128
#define SUBHOUND_MAX_PRESET_LEN       64
#define SUBHOUND_MAX_PATH_LEN         256

typedef struct {
    char preset[SUBHOUND_MAX_PRESET_LEN];
    uint32_t frequency_hz;
    uint32_t te_us;
    uint32_t total_bit_header;
    float lat;
    float lon;
    bool has_gps;

    uint16_t segment_count;
    uint16_t segment_bit_lens[SUBHOUND_MAX_SEGMENTS];
    uint8_t* segment_bits[SUBHOUND_MAX_SEGMENTS];

    /* Inner-bit ranges (after strip_padding); inner_len == 0 means empty. */
    uint16_t inner_start[SUBHOUND_MAX_SEGMENTS];
    uint16_t inner_len[SUBHOUND_MAX_SEGMENTS];

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
    ManchesterDiffTransitionIsOne = 2,
    ManchesterDiffTransitionIsZero = 3,
} ManchesterConvention;

typedef struct {
    /* From SubFile */
    uint32_t frequency;
    float te_us;
    float bitrate_bps;

    uint16_t seg_count;
    uint16_t seg_sizes[SUBHOUND_MAX_SEGMENTS];
    uint32_t total_bits;

    uint16_t inner_sizes[SUBHOUND_MAX_SEGMENTS];
    uint32_t total_inner_bits;
    uint32_t inner_set_bits; /* count of 1s in inner bits (used by NOISE rules) */
    float mean_inner_size;

    float zero_ratio;
    float entropy;

    uint16_t dominant_1run;
    uint16_t dominant_0run;
    float run_variety;

    PWMParams pwm_params;

    uint8_t pwm_decoded_bits[SUBHOUND_MAX_DECODED_BITS];
    uint16_t pwm_decoded_count;

    PreambleInfo preamble;

    bool has_seg_similarity;
    float seg_similarity;

    uint16_t repeating_subpattern_period; /* 0 = none */
    uint16_t repeating_subpattern_reps;

    uint8_t manchester_decoded_bits[SUBHOUND_MAX_MANCH_BITS];
    uint16_t manchester_decoded_count;
    float manchester_error_rate;
    ManchesterConvention manchester_convention;

    bool rolling_code;
    bool fixed_code;
    uint16_t diff_positions[SUBHOUND_MAX_DIFF_POSITIONS];
    uint16_t diff_position_count;
    bool diff_positions_truncated; /* >SUBHOUND_MAX_DIFF_POSITIONS positions exist */

    float signal_quality;

    float lat;
    float lon;
    bool has_gps;

    /* Inter-segment timing (TE us, estimated from padding). 0 when single segment. */
    float inter_segment_gap_us_mean;
    float inter_segment_gap_us_var;

    /* Tri-state PWM detection (set when a third gap bucket carries >=10% of zero-runs). */
    bool pwm3_detected;
    uint16_t pwm3_symbol_count;

    /* CRC trailer scan on PWM-decoded payload. */
    bool crc_valid;
    /* 0 = none, 1 = CRC-8, 2 = CRC-16-CCITT. */
    uint8_t crc_kind;
} FeatureVector;

typedef enum {
    SubhoundLabelNoise = 0,
    SubhoundLabelAmrMeter,
    SubhoundLabelTpms,
    SubhoundLabelWmbusMeter,
    SubhoundLabelHoneywell5800,
    SubhoundLabelAlarmSensor,
    SubhoundLabelShutterBlind,
    SubhoundLabelEnoceanSwitch,
    SubhoundLabelDoorbell,
    SubhoundLabelOutletSwitch,
    SubhoundLabelGarageRemote,
    SubhoundLabelKeyfobRemote,
    SubhoundLabelWeatherStation,
    SubhoundLabelLoraBeacon,
    SubhoundLabelPt2262Remote,
    SubhoundLabelEv1527Remote,
    SubhoundLabelUnknownStructured,
} SubhoundLabel;

typedef enum {
    SubhoundConfLow = 0,
    SubhoundConfMedium,
    SubhoundConfHigh,
} SubhoundConfidence;

typedef struct {
    SubhoundLabel label;
    SubhoundConfidence confidence;

    char hints[SUBHOUND_MAX_HINTS][SUBHOUND_MAX_HINT_LEN];
    uint8_t hint_count;

    char reasons[SUBHOUND_MAX_REASONS][SUBHOUND_MAX_REASON_LEN];
    uint8_t reason_count;

    char warnings[SUBHOUND_MAX_WARNINGS][SUBHOUND_MAX_WARNING_LEN];
    uint8_t warning_count;
} ClassificationResult;

const char* subhound_label_name(SubhoundLabel label);
const char* subhound_confidence_name(SubhoundConfidence c);
const char* subhound_manchester_name(ManchesterConvention c);
