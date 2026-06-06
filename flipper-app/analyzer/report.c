#include "report.h"
#include "bits.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Stream hex bytes into a FuriString without a large stack buffer. */
static void cat_hex_bits(FuriString* out, const uint8_t* bits, uint16_t len) {
    if(len == 0) return;
    uint16_t total_bits = (uint16_t)((len + 7u) / 8u * 8u);
    for(uint16_t i = 0; i < total_bits; i += 8) {
        uint8_t byte = 0;
        for(uint8_t j = 0; j < 8; j++) {
            uint8_t bit = (uint16_t)(i + j) < len ? bits[i + j] : 0;
            byte = (uint8_t)((byte << 1) | (bit & 1u));
        }
        furi_string_cat_printf(out, i == 0 ? "%02X" : " %02X", byte);
    }
}

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

/* ------------------- section builders (composable) -------------------- */

static void cat_header(
    const char* path,
    const ClassificationResult* result,
    FuriString* out) {
    furi_string_cat_str(out, SEP);
    furi_string_cat_printf(out, "FILE: %s\n", basename_of(path));
    furi_string_cat_str(out, SEP);
    furi_string_cat_str(out, "\n");

    furi_string_cat_printf(out, "CLASSIFICATION : %s\n", subhound_label_name(result->label));
    furi_string_cat_printf(
        out, "CONFIDENCE     : %s\n", subhound_confidence_name(result->confidence));

    for(uint8_t i = 0; i < result->hint_count; i++) {
        furi_string_cat_printf(
            out,
            "%s %s\n",
            i == 0 ? "SUB-PROTOCOL   :" : "               :",
            result->hints[i]);
    }
}

static void cat_metrics(const FeatureVector* fv, FuriString* out) {
    char freq_buf[32];
    freq_label(fv->frequency, freq_buf, sizeof(freq_buf));

    furi_string_cat_str(out, "KEY METRICS\n");
    furi_string_cat_printf(out, "  Frequency    : %s\n", freq_buf);
    furi_string_cat_printf(
        out,
        "  TE           : %.0f us  ->  bitrate ~ %.0f bps\n",
        (double)fv->te_us,
        (double)fv->bitrate_bps);

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
        if(fv->inter_segment_gap_us_mean > 0.0f) {
            float jitter = fv->inter_segment_gap_us_var > 0.0f
                               ? sqrtf(fv->inter_segment_gap_us_var)
                               : 0.0f;
            furi_string_cat_printf(
                out,
                "  Inter-seg gap: mean %.1f ms  (jitter %.1f ms)\n",
                (double)(fv->inter_segment_gap_us_mean / 1000.0f),
                (double)(jitter / 1000.0f));
        }
    }

    if(fv->pwm3_detected) {
        furi_string_cat_printf(
            out, "  Tri-state PWM: detected (%u symbols)\n", fv->pwm3_symbol_count);
    }
    if(fv->crc_valid) {
        furi_string_cat_printf(
            out,
            "  CRC          : %s valid\n",
            fv->crc_kind == 1 ? "CRC-8" : "CRC-16-CCITT");
    }

    if(fv->has_gps) {
        furi_string_cat_printf(
            out, "  Location     : %.6f, %.6f\n", (double)fv->lat, (double)fv->lon);
    }
}

static void cat_payload(const FeatureVector* fv, FuriString* out) {
    if(!fv->pwm_params.found || fv->pwm_decoded_count == 0) {
        furi_string_cat_str(out, "PAYLOAD\n  (no PWM payload decoded)\n");
        return;
    }
    furi_string_cat_str(out, "PAYLOAD\n");
    furi_string_cat_printf(out, "  Bits (%u): ", fv->pwm_decoded_count);
    for(uint16_t i = 0; i < fv->pwm_decoded_count; i++) {
        furi_string_cat_printf(out, "%c", fv->pwm_decoded_bits[i] ? '1' : '0');
    }
    furi_string_cat_str(out, "\n  Hex : ");
    cat_hex_bits(out, fv->pwm_decoded_bits, fv->pwm_decoded_count);
    furi_string_cat_str(out, "\n");
}

static void cat_manchester(const FeatureVector* fv, FuriString* out) {
    if(fv->manchester_decoded_count == 0 || fv->manchester_error_rate >= 0.30f) {
        furi_string_cat_str(out, "MANCHESTER\n  (no clean Manchester decode)\n");
        return;
    }
    furi_string_cat_printf(
        out,
        "MANCHESTER\n  %u bits decoded [%s, %.0f%% errors]\n  Hex : ",
        fv->manchester_decoded_count,
        subhound_manchester_name(fv->manchester_convention),
        (double)(fv->manchester_error_rate * 100.0f));
    cat_hex_bits(out, fv->manchester_decoded_bits, fv->manchester_decoded_count);
    furi_string_cat_str(out, "\n");
}

static void cat_reasoning(const ClassificationResult* result, FuriString* out) {
    furi_string_cat_str(out, "REASONING CHAIN\n");
    for(uint8_t i = 0; i < result->reason_count; i++) {
        furi_string_cat_printf(out, "  %s\n", result->reasons[i]);
    }
}

static void cat_warnings(const ClassificationResult* result, FuriString* out) {
    if(result->warning_count == 0) {
        furi_string_cat_str(out, "WARNINGS\n  (none)\n");
        return;
    }
    furi_string_cat_str(out, "WARNINGS\n");
    for(uint8_t i = 0; i < result->warning_count; i++) {
        furi_string_cat_printf(out, "  ! %s\n", result->warnings[i]);
    }
}

/* ----------------------------- public API ---------------------------- */

void report_format(
    const char* path,
    const SubFile* sub,
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out) {
    (void)sub;
    cat_header(path, result, out);

    furi_string_cat_str(out, "\n");
    cat_metrics(fv, out);

    /* Payload + Manchester are inline in the full report (preserving original
     * .report.txt format). The metrics block already prints PWM stats; we append
     * the bit-stream / hex blobs here. */
    if(fv->pwm_params.found && fv->pwm_decoded_count > 0) {
        furi_string_cat_str(out, "  Payload bits : ");
        for(uint16_t i = 0; i < fv->pwm_decoded_count; i++) {
            furi_string_cat_printf(out, "%c", fv->pwm_decoded_bits[i] ? '1' : '0');
        }
        furi_string_cat_str(out, "\n  Payload hex  : ");
        cat_hex_bits(out, fv->pwm_decoded_bits, fv->pwm_decoded_count);
        furi_string_cat_str(out, "\n");
    }
    if(fv->manchester_decoded_count > 0 && fv->manchester_error_rate < 0.30f) {
        furi_string_cat_printf(
            out,
            "  Manchester   : %u bits decoded [%s, %.0f%% errors]\n  Manch. hex   : ",
            fv->manchester_decoded_count,
            subhound_manchester_name(fv->manchester_convention),
            (double)(fv->manchester_error_rate * 100.0f));
        cat_hex_bits(out, fv->manchester_decoded_bits, fv->manchester_decoded_count);
        furi_string_cat_str(out, "\n");
    }

    furi_string_cat_str(out, "\n");
    cat_reasoning(result, out);

    if(result->warning_count > 0) {
        furi_string_cat_str(out, "\n");
        furi_string_cat_str(out, "WARNINGS\n");
        for(uint8_t i = 0; i < result->warning_count; i++) {
            furi_string_cat_printf(out, "  ! %s\n", result->warnings[i]);
        }
    }

    furi_string_cat_str(out, "\n");
}

void report_format_section(
    ReportSection section,
    const char* path,
    const SubFile* sub,
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out) {
    (void)sub;
    (void)path;
    switch(section) {
    case ReportSectionMetrics:
        cat_metrics(fv, out);
        break;
    case ReportSectionReasoning:
        cat_reasoning(result, out);
        break;
    case ReportSectionPayload:
        cat_payload(fv, out);
        break;
    case ReportSectionManchester:
        cat_manchester(fv, out);
        break;
    case ReportSectionWarnings:
        cat_warnings(result, out);
        break;
    case ReportSectionFull:
        report_format(path, sub, fv, result, out);
        break;
    }
}

bool report_section_has_content(
    ReportSection section,
    const FeatureVector* fv,
    const ClassificationResult* result) {
    switch(section) {
    case ReportSectionMetrics:
    case ReportSectionReasoning:
    case ReportSectionFull:
        return true;
    case ReportSectionPayload:
        return fv->pwm_params.found && fv->pwm_decoded_count > 0;
    case ReportSectionManchester:
        return fv->manchester_decoded_count > 0 && fv->manchester_error_rate < 0.30f;
    case ReportSectionWarnings:
        return result->warning_count > 0;
    }
    return false;
}

void report_format_summary_lines(
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out) {
    char freq_buf[16];
    freq_label(fv->frequency, freq_buf, sizeof(freq_buf));

    /* Line 1: freq · TE · segs */
    furi_string_cat_printf(
        out,
        "%s  TE %.0fus  %u seg\n",
        freq_buf,
        (double)fv->te_us,
        fv->seg_count);

    /* Line 2: signal quality · code type · similarity (when available) */
    furi_string_cat_printf(out, "sig %.0f%%", (double)(fv->signal_quality * 100.0f));
    if(fv->has_seg_similarity) {
        furi_string_cat_printf(out, "  sim %.0f%%", (double)(fv->seg_similarity * 100.0f));
    }
    if(fv->rolling_code) {
        furi_string_cat_printf(out, "  ROLL(%u)", fv->diff_position_count);
    } else if(fv->fixed_code && fv->seg_count >= 2) {
        furi_string_cat_str(out, "  FIXED");
    }
    furi_string_cat_str(out, "\n");

    /* Line 3: first sub-protocol hint (if any) */
    if(result->hint_count > 0) {
        furi_string_cat_printf(out, "%s\n", result->hints[0]);
    }

    /* Line 4: CRC/Manchester/PWM3 flag, only if something interesting */
    bool wrote_extra = false;
    if(fv->crc_valid) {
        furi_string_cat_printf(
            out, "CRC %s ok", fv->crc_kind == 1 ? "8" : "16");
        wrote_extra = true;
    }
    if(fv->pwm3_detected) {
        furi_string_cat_str(out, wrote_extra ? "  +Tri-PWM" : "Tri-PWM detected");
        wrote_extra = true;
    }
    if(wrote_extra) furi_string_cat_str(out, "\n");
}
