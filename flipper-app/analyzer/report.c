#include "report.h"
#include "bits.h"
#include <string.h>

#define SEP                                                       \
    "============================================================" \
    "\n"

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void freq_label(uint32_t hz, char* out, size_t cap) {
    if(hz >= 1000000u) {
        if(hz % 1000000u == 0) {
            snprintf(out, cap, "%lu MHz", (unsigned long)(hz / 1000000u));
        } else {
            float mhz = (float)hz / 1000000.0f;
            snprintf(out, cap, "%.3f MHz", (double)mhz);
            /* strip trailing zeros */
            char* dot = strchr(out, '.');
            if(dot) {
                char* end = out + strlen(out) - 1;
                while(end > dot && *end == '0') *end-- = 0;
                if(end == dot) *end = 0;
            }
        }
    } else {
        snprintf(out, cap, "%lu Hz", (unsigned long)hz);
    }
}

void report_format(
    const char* path,
    const SubFile* sub,
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out) {
    (void)sub;
    char freq_buf[32];
    freq_label(fv->frequency, freq_buf, sizeof(freq_buf));

    furi_string_cat_str(out, SEP);
    furi_string_cat_printf(out, "FILE: %s\n", basename_of(path));
    furi_string_cat_str(out, SEP);
    furi_string_cat_str(out, "\n");

    furi_string_cat_printf(out, "CLASSIFICATION : %s\n", bitraw_label_name(result->label));
    furi_string_cat_printf(out, "CONFIDENCE     : %s\n", bitraw_confidence_name(result->confidence));

    for(uint8_t i = 0; i < result->hint_count; i++) {
        furi_string_cat_printf(
            out,
            "%s %s\n",
            i == 0 ? "SUB-PROTOCOL   :" : "               :",
            result->hints[i]);
    }

    furi_string_cat_str(out, "\n");
    furi_string_cat_str(out, "KEY METRICS\n");
    furi_string_cat_printf(out, "  Frequency    : %s\n", freq_buf);
    furi_string_cat_printf(
        out,
        "  TE           : %.0f us  ->  bitrate ~ %.0f bps\n",
        (double)fv->te_us,
        (double)fv->bitrate_bps);

    /* Segment sizes */
    furi_string_cat_printf(out, "  Segments     : %u  (sizes: ", fv->seg_count);
    for(uint16_t i = 0; i < fv->seg_count; i++) {
        furi_string_cat_printf(out, "%s%u", i ? ", " : "", fv->seg_sizes[i]);
    }
    furi_string_cat_str(out, " bits)\n");

    if(fv->has_seg_similarity) {
        furi_string_cat_printf(
            out, "  Segment sim  : %.1f%% identical\n", (double)(fv->seg_similarity * 100.0f));
    } else {
        furi_string_cat_str(out, "  Segment sim  : n/a (single segment)\n");
    }

    furi_string_cat_printf(
        out,
        "  Inner bits   : %lu total  (mean %.0f/segment)\n",
        (unsigned long)fv->total_inner_bits,
        (double)fv->mean_inner_size);
    furi_string_cat_printf(
        out, "  Zero ratio   : %.1f%% (of raw bits)\n", (double)(fv->zero_ratio * 100.0f));
    furi_string_cat_printf(out, "  Entropy      : %.3f bits/symbol\n", (double)fv->entropy);

    if(fv->pwm_params.found) {
        furi_string_cat_printf(
            out,
            "  PWM          : pulse=%u TE, short_gap=%u TE, long_gap=%u TE  [consistency: %.0f%%]\n",
            fv->pwm_params.pulse_width,
            fv->pwm_params.short_gap,
            fv->pwm_params.long_gap,
            (double)(fv->pwm_params.consistency * 100.0f));
        furi_string_cat_printf(out, "  Decoded bits : %u\n", fv->pwm_decoded_count);
        if(fv->pwm_decoded_count > 0) {
            furi_string_cat_str(out, "  Payload bits : ");
            for(uint16_t i = 0; i < fv->pwm_decoded_count; i++) {
                furi_string_cat_printf(out, "%c", fv->pwm_decoded_bits[i] ? '1' : '0');
            }
            furi_string_cat_str(out, "\n");
            char hex[BITRAW_MAX_DECODED_BITS / 8 * 4 + 8];
            bits_to_hex(fv->pwm_decoded_bits, fv->pwm_decoded_count, hex, sizeof(hex));
            furi_string_cat_printf(out, "  Payload hex  : %s\n", hex);
        }
    } else {
        furi_string_cat_str(out, "  PWM          : not detected\n");
    }

    if(fv->preamble.found) {
        furi_string_cat_printf(
            out,
            "  Preamble     : %u-bit alternating at offset %u\n",
            fv->preamble.length,
            fv->preamble.position);
    } else {
        furi_string_cat_str(out, "  Preamble     : not detected\n");
    }

    furi_string_cat_printf(
        out, "  Signal qual  : %.0f%%\n", (double)(fv->signal_quality * 100.0f));

    if(fv->manchester_decoded_count > 0 && fv->manchester_error_rate < 0.30f) {
        furi_string_cat_printf(
            out,
            "  Manchester   : %u bits decoded [%s, %.0f%% errors]\n",
            fv->manchester_decoded_count,
            bitraw_manchester_name(fv->manchester_convention),
            (double)(fv->manchester_error_rate * 100.0f));
        char hex[BITRAW_MAX_MANCH_BITS / 8 * 4 + 8];
        bits_to_hex(
            fv->manchester_decoded_bits, fv->manchester_decoded_count, hex, sizeof(hex));
        furi_string_cat_printf(out, "  Manch. hex   : %s\n", hex);
    }

    if(fv->seg_count >= 2) {
        if(fv->rolling_code) {
            furi_string_cat_printf(
                out,
                "  Code type    : ROLLING (changes at %u bit positions)\n",
                fv->diff_position_count);
        } else if(fv->fixed_code) {
            furi_string_cat_printf(
                out,
                "  Code type    : FIXED (identical across all %u segments)\n",
                fv->seg_count);
        }
    }

    if(fv->has_gps) {
        furi_string_cat_printf(
            out, "  Location     : %.6f, %.6f\n", (double)fv->lat, (double)fv->lon);
    }

    furi_string_cat_str(out, "\n");
    furi_string_cat_str(out, "REASONING CHAIN\n");
    for(uint8_t i = 0; i < result->reason_count; i++) {
        furi_string_cat_printf(out, "  %s\n", result->reasons[i]);
    }

    if(result->warning_count > 0) {
        furi_string_cat_str(out, "\n");
        furi_string_cat_str(out, "WARNINGS\n");
        for(uint8_t i = 0; i < result->warning_count; i++) {
            furi_string_cat_printf(out, "  ! %s\n", result->warnings[i]);
        }
    }

    furi_string_cat_str(out, "\n");
}
