#include "classifier.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void classifier_add_reason(ClassificationResult* r, const char* fmt, ...) {
    if(r->reason_count >= BITRAW_MAX_REASONS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->reasons[r->reason_count], BITRAW_MAX_REASON_LEN, fmt, ap);
    va_end(ap);
    r->reason_count++;
}

void classifier_add_hint(ClassificationResult* r, const char* fmt, ...) {
    if(r->hint_count >= BITRAW_MAX_HINTS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->hints[r->hint_count], BITRAW_MAX_HINT_LEN, fmt, ap);
    va_end(ap);
    r->hint_count++;
}

void classifier_add_warning(ClassificationResult* r, const char* fmt, ...) {
    if(r->warning_count >= BITRAW_MAX_WARNINGS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->warnings[r->warning_count], BITRAW_MAX_WARNING_LEN, fmt, ap);
    va_end(ap);
    r->warning_count++;
}

static void reset_result(ClassificationResult* r) {
    memset(r, 0, sizeof(*r));
}

static bool is_ism_freq(uint32_t hz) {
    return hz == 315000000u || hz == 433420000u || hz == 433920000u ||
           hz == 434420000u || hz == 868350000u || hz == 915000000u;
}

/* ============================== NOISE ================================== */

static bool classify_noise(const FeatureVector* fv, ClassificationResult* out) {
    uint32_t set_bits = fv->inner_set_bits;

    if(set_bits <= 2) {
        out->label = BitrawLabelNoise;
        out->confidence = BitrawConfHigh;
        classifier_add_reason(
            out, "[N0] Only %lu set bit(s) - effectively empty capture", (unsigned long)set_bits);
        return true;
    }
    if(fv->total_bits < 50) {
        out->label = BitrawLabelNoise;
        out->confidence = BitrawConfHigh;
        classifier_add_reason(
            out,
            "[N1] Only %lu bits total - too short to be a real signal",
            (unsigned long)fv->total_bits);
        return true;
    }
    if(fv->zero_ratio > 0.97f && set_bits < 20) {
        out->label = BitrawLabelNoise;
        out->confidence = BitrawConfHigh;
        classifier_add_reason(
            out,
            "[N2] zero_ratio=%.1f%%, only %lu set bits - background noise",
            (double)(fv->zero_ratio * 100.0f),
            (unsigned long)set_bits);
        return true;
    }
    if(fv->seg_count == 1 && fv->zero_ratio > 0.95f && fv->entropy < 0.25f) {
        out->label = BitrawLabelNoise;
        out->confidence = BitrawConfHigh;
        classifier_add_reason(
            out,
            "[N3] Single segment, zero_ratio=%.1f%% > 95%%, entropy=%.3f < 0.25",
            (double)(fv->zero_ratio * 100.0f),
            (double)fv->entropy);
        return true;
    }
    return false;
}

/* ============================== AMR_METER ============================== */

static bool classify_amr_meter(const FeatureVector* fv, ClassificationResult* out) {
    if(fv->frequency != 315000000u && fv->frequency != 868350000u) return false;

    int score = 0;

    if(fv->preamble.found && fv->preamble.length >= 16) {
        score += 3;
        classifier_add_reason(
            out, "[AMR1] %u-bit alternating preamble - AMR/ERT sync pattern", fv->preamble.length);
    }
    if(fv->manchester_decoded_count >= 60 && fv->manchester_error_rate < 0.15f) {
        score += 3;
        classifier_add_reason(
            out,
            "[AMR2] Manchester decode: %u bits, %.0f%% errors - ERT payload",
            fv->manchester_decoded_count,
            (double)(fv->manchester_error_rate * 100.0f));
    }
    if(fv->total_inner_bits >= 200) {
        score += 2;
        classifier_add_reason(
            out,
            "[AMR3] %lu inner bits - AMR frames are 600-800 bits",
            (unsigned long)fv->total_inner_bits);
    }
    if(fv->te_us >= 40.0f && fv->te_us <= 100.0f) {
        score += 1;
        classifier_add_reason(
            out, "[AMR4] TE=%.0fus - fast OOK typical of ERT (~50us)", (double)fv->te_us);
    }
    if(fv->seg_count == 1) {
        score += 1;
        classifier_add_reason(
            out, "[AMR5] Single burst - ERT meters transmit once per interval");
    }

    if(score < 4) {
        out->reason_count = 0;
        return false;
    }

    if(fv->frequency == 315000000u) {
        classifier_add_hint(out, "315MHz -> US ERT (Encoder Receiver Transmitter) profile");
    } else {
        classifier_add_hint(out, "868MHz -> EU M-Bus / wireless meter profile");
    }
    if(fv->manchester_decoded_count > 0) {
        classifier_add_hint(
            out, "Manchester payload: %u bits decoded", fv->manchester_decoded_count);
    }

    out->label = BitrawLabelAmrMeter;
    out->confidence = score >= 7 ? BitrawConfMedium : BitrawConfLow;
    classifier_add_warning(
        out, "AMR identification requires CRC validation against ERT/IRTIS spec");
    return true;
}

/* ============================== TPMS =================================== */

static bool classify_tpms(const FeatureVector* fv, ClassificationResult* out) {
    if(!is_ism_freq(fv->frequency)) return false;

    if(fv->pwm_params.found && fv->pwm_params.consistency > 0.85f &&
       fv->pwm_decoded_count < 80) {
        return false;
    }
    if(fv->has_seg_similarity && fv->seg_similarity > 0.92f) {
        return false;
    }

    int score = 0;
    if(fv->te_us >= 50.0f && fv->te_us <= 200.0f) {
        score += 2;
        classifier_add_reason(
            out, "[T1] TE=%.0fus within TPMS range (50-200us)", (double)fv->te_us);
    }
    if(fv->seg_count >= 2 && fv->seg_count <= 8) {
        score += 2;
        classifier_add_reason(
            out, "[T2] %u segments - TPMS sensors repeat 3-8x", fv->seg_count);
    }
    if(fv->mean_inner_size >= 60.0f && fv->mean_inner_size <= 200.0f) {
        score += 2;
        classifier_add_reason(
            out,
            "[T3] Mean inner size %.0f bits - matches TPMS burst length (60-200)",
            (double)fv->mean_inner_size);
    }
    if(fv->has_seg_similarity && fv->seg_similarity >= 0.70f && fv->seg_similarity <= 0.99f) {
        score += 2;
        classifier_add_reason(
            out,
            "[T4] Segment similarity %.1f%% - similar but not identical (rolling counter)",
            (double)(fv->seg_similarity * 100.0f));
    }
    if(fv->zero_ratio >= 0.80f && fv->zero_ratio <= 0.97f) {
        score += 1;
        classifier_add_reason(
            out, "[T5] zero_ratio=%.1f%% - typical preamble/silence framing",
            (double)(fv->zero_ratio * 100.0f));
    }
    if(fv->entropy >= 0.35f && fv->entropy <= 0.75f) {
        score += 1;
        classifier_add_reason(
            out, "[T6] entropy=%.3f - structured but not fully random", (double)fv->entropy);
    }

    int min_score = fv->seg_count == 1 ? 6 : 4;
    if(score < min_score) {
        out->reason_count = 0;
        return false;
    }

    BitrawConfidence conf;
    if(fv->frequency == 315000000u) {
        classifier_add_hint(out, "315MHz -> North American TPMS standard");
        conf = score >= 8 ? BitrawConfHigh : (score >= 6 ? BitrawConfMedium : BitrawConfLow);
    } else {
        classifier_add_hint(out, "433.92MHz -> European TPMS standard");
        conf = score >= 6 ? BitrawConfMedium : BitrawConfLow;
    }

    if(fv->seg_count == 1) {
        classifier_add_warning(
            out, "Single segment - cannot confirm repeat burst; may be partial capture");
    }

    out->label = BitrawLabelTpms;
    out->confidence = conf;
    return true;
}

/* ============================ ALARM_SENSOR ============================= */

static bool classify_alarm_sensor(const FeatureVector* fv, ClassificationResult* out) {
    if(fv->frequency != 433920000u && fv->frequency != 868350000u) return false;
    if(fv->pwm_params.found && fv->pwm_params.consistency > 0.80f) return false;
    if(fv->seg_count > 3) return false;
    if(fv->has_seg_similarity && fv->seg_similarity > 0.92f) return false;
    if(fv->entropy < 0.90f) return false;

    int score = 0;
    if(fv->te_us >= 100.0f && fv->te_us <= 400.0f) {
        score += 2;
        classifier_add_reason(
            out,
            "[AS1] TE=%.0fus - alarm sensor OOK range (100-400us)",
            (double)fv->te_us);
    }
    if(fv->mean_inner_size >= 40.0f && fv->mean_inner_size <= 120.0f) {
        score += 2;
        classifier_add_reason(
            out,
            "[AS2] Inner size %.0f bits - alarm payload range (40-120)",
            (double)fv->mean_inner_size);
    }
    if(fv->entropy >= 0.90f) {
        score += 2;
        classifier_add_reason(
            out,
            "[AS3] entropy=%.3f - high entropy consistent with encrypted payload",
            (double)fv->entropy);
    }
    if(fv->zero_ratio >= 0.40f && fv->zero_ratio <= 0.75f) {
        score += 1;
        classifier_add_reason(
            out, "[AS4] zero_ratio=%.1f%% - dense signal data",
            (double)(fv->zero_ratio * 100.0f));
    }
    if(fv->seg_count <= 2) {
        score += 1;
        classifier_add_reason(
            out, "[AS5] %u segment(s) - alarm sensors transmit 1-2x per event", fv->seg_count);
    }

    if(score < 7) {
        out->reason_count = 0;
        return false;
    }

    if(fv->te_us >= 130.0f && fv->te_us <= 170.0f) {
        classifier_add_hint(out, "TE ~150us -> possible Honeywell 5800-series");
    } else if(fv->te_us >= 90.0f && fv->te_us <= 130.0f) {
        classifier_add_hint(out, "TE ~110us -> possible DSC/Visonic");
    }
    if(fv->manchester_error_rate < 0.20f && fv->manchester_decoded_count > 20) {
        classifier_add_hint(
            out,
            "Manchester decode ok (%u bits, %.0f%% err)",
            fv->manchester_decoded_count,
            (double)(fv->manchester_error_rate * 100.0f));
    }
    if(fv->frequency == 868350000u) {
        classifier_add_hint(out, "868MHz -> European alarm sensor band");
    } else {
        classifier_add_hint(out, "433.92MHz -> alarm sensor ISM band");
    }

    out->label = BitrawLabelAlarmSensor;
    out->confidence = score >= 8 ? BitrawConfMedium : BitrawConfLow;
    classifier_add_warning(
        out, "Cannot confirm alarm brand without protocol-specific CRC check");
    return true;
}

/* =========================== SHUTTER_BLIND ============================= */

static bool classify_shutter_blind(const FeatureVector* fv, ClassificationResult* out) {
    if(fv->frequency != 433420000u && fv->frequency != 433920000u) return false;
    if(fv->te_us < 550.0f || fv->te_us > 700.0f) return false;

    classifier_add_reason(
        out, "[SB1] TE=%.0fus - matches Somfy RTS timing (TE~604us)", (double)fv->te_us);

    if(fv->total_inner_bits >= 50 && fv->total_inner_bits <= 80) {
        classifier_add_reason(
            out,
            "[SB2] %lu inner bits - Somfy RTS 56-bit payload range",
            (unsigned long)fv->total_inner_bits);
    }
    if(fv->frequency == 433420000u) {
        classifier_add_hint(out, "433.42MHz -> Somfy RTS confirmed frequency");
    } else {
        classifier_add_hint(out, "433.92MHz capture - Somfy RTS is 433.42MHz; may be degraded");
        classifier_add_warning(
            out, "Frequency offset +/-500kHz: confirm with Flipper tuned to 433.42MHz");
    }
    if(fv->seg_count >= 2) {
        classifier_add_hint(
            out, "%u segments - Somfy typically transmits 2x with pause", fv->seg_count);
    }

    out->label = BitrawLabelShutterBlind;
    out->confidence = fv->frequency == 433420000u ? BitrawConfMedium : BitrawConfLow;
    return true;
}

/* ============================== DOORBELL =============================== */

static bool classify_doorbell(const FeatureVector* fv, ClassificationResult* out) {
    if(!is_ism_freq(fv->frequency)) return false;
    if(fv->seg_count < 5 || fv->seg_count > 10) return false;
    if(!fv->has_seg_similarity || fv->seg_similarity < 0.92f) return false;
    if(!fv->pwm_params.found || fv->pwm_params.consistency < 0.75f) return false;
    if(fv->pwm_decoded_count < 16 || fv->pwm_decoded_count > 40) return false;

    classifier_add_reason(
        out, "[D1] %u repeats - doorbells transmit 5-8x while button held", fv->seg_count);
    classifier_add_reason(
        out,
        "[D2] Segment similarity %.1f%% - identical fixed code",
        (double)(fv->seg_similarity * 100.0f));
    classifier_add_reason(
        out, "[D3] %u decoded bits - PT2262 fixed-code family", fv->pwm_decoded_count);
    if(fv->te_us >= 100.0f && fv->te_us <= 400.0f) {
        classifier_add_reason(
            out, "[D4] TE=%.0fus - OOK doorbell range", (double)fv->te_us);
    }

    classifier_add_hint(out, "PT2262 fixed-code doorbell");
    if(fv->frequency == 433920000u) {
        classifier_add_hint(out, "433.92MHz -> European/universal doorbell");
    } else {
        classifier_add_hint(out, "315MHz -> North American doorbell");
    }

    out->label = BitrawLabelDoorbell;
    out->confidence = BitrawConfMedium;
    return true;
}

/* =========================== OUTLET_SWITCH ============================= */

static bool classify_outlet_switch(const FeatureVector* fv, ClassificationResult* out) {
    if(!is_ism_freq(fv->frequency)) return false;
    if(fv->seg_count < 3 || fv->seg_count > 4) return false;
    if(!fv->has_seg_similarity || fv->seg_similarity < 0.97f) return false;
    if(!fv->pwm_params.found || fv->pwm_params.consistency < 0.80f) return false;
    if(fv->pwm_decoded_count < 24 || fv->pwm_decoded_count > 32) return false;
    if(fv->rolling_code) return false;

    classifier_add_reason(
        out, "[O1] %u repeats - wireless outlets transmit 3-4x", fv->seg_count);
    classifier_add_reason(
        out,
        "[O2] Segment similarity %.1f%% - perfectly identical (fixed code)",
        (double)(fv->seg_similarity * 100.0f));
    classifier_add_reason(
        out, "[O3] %u decoded bits - PT2262 24-bit fixed code", fv->pwm_decoded_count);
    classifier_add_reason(
        out, "[O4] No rolling code detected - consistent with static outlet address");

    classifier_add_hint(out, "PT2262 wireless outlet/switch");
    classifier_add_hint(
        out, fv->frequency == 433920000u ? "433.92MHz ISM" : "315MHz ISM");
    classifier_add_warning(
        out,
        "Cannot distinguish outlet from short garage/barrier remote without brand DB");

    out->label = BitrawLabelOutletSwitch;
    out->confidence = BitrawConfLow;
    return true;
}

/* =========================== GARAGE_REMOTE ============================= */

static bool classify_garage(const FeatureVector* fv, ClassificationResult* out) {
    if(!is_ism_freq(fv->frequency)) return false;
    if(fv->seg_count < 2 || fv->seg_count > 6) return false;
    if(!fv->has_seg_similarity || fv->seg_similarity < 0.92f) return false;

    classifier_add_reason(
        out, "[G1] %u segments - consistent with %ux repeat", fv->seg_count, fv->seg_count);
    classifier_add_reason(
        out,
        "[G2] Segment similarity %.1f%% - near-identical copies",
        (double)(fv->seg_similarity * 100.0f));

    bool has_pwm = fv->pwm_params.found && fv->pwm_params.consistency >= 0.80f;
    int strong_count = 0;

    if(has_pwm) {
        strong_count++;
        classifier_add_reason(
            out,
            "[G3] Clean PWM (pulse=%u, short=%u, long=%u TE)",
            fv->pwm_params.pulse_width,
            fv->pwm_params.short_gap,
            fv->pwm_params.long_gap);
        classifier_add_reason(
            out,
            "[G4] PWM consistency %.0f%% - well-formed symbols",
            (double)(fv->pwm_params.consistency * 100.0f));
    }
    if(fv->te_us >= 100.0f && fv->te_us <= 400.0f) {
        classifier_add_reason(
            out, "[G5] TE=%.0fus within OOK remote range (100-400us)", (double)fv->te_us);
    }
    if(has_pwm && fv->pwm_decoded_count >= 24 && fv->pwm_decoded_count <= 80) {
        classifier_add_reason(
            out,
            "[G6] %u decoded payload bits - consistent with remote frame size",
            fv->pwm_decoded_count);
    }
    if(fv->zero_ratio >= 0.60f && fv->zero_ratio <= 0.85f) {
        classifier_add_reason(
            out,
            "[G7] zero_ratio=%.1f%% - typical OOK framing overhead",
            (double)(fv->zero_ratio * 100.0f));
    }

    BitrawConfidence conf;
    if(strong_count >= 1) {
        conf = BitrawConfHigh;
    } else {
        conf = BitrawConfLow;
        classifier_add_warning(
            out, "Near-identical repeats but no clean PWM - encoding ambiguous");
    }

    if(fv->rolling_code) {
        char buf[64];
        size_t pos = 0;
        uint16_t shown = fv->diff_position_count < 8 ? fv->diff_position_count : 8;
        for(uint16_t i = 0; i < shown; i++) {
            int n = snprintf(
                buf + pos,
                sizeof(buf) - pos,
                "%s%u",
                i ? "," : "",
                fv->diff_positions[i]);
            if(n < 0 || (size_t)n >= sizeof(buf) - pos) break;
            pos += (size_t)n;
        }
        classifier_add_reason(
            out,
            "[G8] ROLLING CODE (changes at: %s%s)",
            buf,
            fv->diff_position_count > 8 ? "..." : "");
    } else if(fv->fixed_code && fv->seg_count >= 2) {
        classifier_add_reason(
            out, "[G8] FIXED CODE - identical payload (potentially replayable)");
        classifier_add_warning(
            out, "Fixed code detected - this transmission may be vulnerable to replay");
    }

    uint16_t dc = fv->pwm_decoded_count;
    if(dc == 66 && fv->seg_count == 3) {
        classifier_add_hint(out, "KeeLoq rolling code (66-bit frame)");
    } else if(dc >= 50 && dc <= 56 && fv->te_us >= 280.0f && fv->te_us <= 380.0f) {
        classifier_add_hint(out, "CAME 52-bit profile");
    } else if(dc >= 24 && dc <= 40 && fv->fixed_code) {
        classifier_add_hint(out, "PT2262/generic fixed-code remote");
    } else if(dc >= 24 && dc <= 40 && fv->rolling_code) {
        classifier_add_hint(out, "Generic rolling-code remote (short frame)");
    } else if(dc >= 24 && dc <= 40) {
        classifier_add_hint(out, "PT2262 family / short remote (need 2+ segs to confirm)");
    } else if(dc > 0) {
        classifier_add_hint(out, "Unrecognised frame (%u decoded bits)", dc);
    }
    if(fv->frequency == 315000000u) {
        classifier_add_hint(out, "315MHz -> N. American garage/barrier/car remote");
    } else {
        classifier_add_hint(out, "433.92MHz -> European garage/CAME/Marantec/car remote");
    }

    if(fv->seg_count > 1) {
        uint32_t sum = 0;
        for(uint16_t i = 0; i < fv->seg_count; i++) sum += fv->seg_sizes[i];
        float mean = (float)sum / (float)fv->seg_count;
        for(uint16_t i = 0; i < fv->seg_count; i++) {
            if((float)fv->seg_sizes[i] > mean * 3.0f) {
                classifier_add_warning(
                    out,
                    "Segment %u (%u bits) is %.1fx larger than avg",
                    i + 1,
                    fv->seg_sizes[i],
                    (double)((float)fv->seg_sizes[i] / mean));
            }
        }
    }

    out->label = BitrawLabelGarageRemote;
    out->confidence = conf;
    return true;
}

/* ============================ KEYFOB_REMOTE ============================ */

static bool classify_keyfob(const FeatureVector* fv, ClassificationResult* out) {
    if(fv->frequency != 315000000u && fv->frequency != 433920000u) return false;

    int rules = 0;
    if(fv->te_us >= 80.0f && fv->te_us <= 500.0f) {
        rules++;
        classifier_add_reason(
            out, "[K1] TE=%.0fus within keyfob OOK range (80-500us)", (double)fv->te_us);
    }
    if(fv->pwm_params.found) {
        rules++;
        classifier_add_reason(out, "[K2] PWM encoding detected");
    }
    if(fv->pwm_decoded_count >= 16 && fv->pwm_decoded_count <= 48) {
        rules++;
        classifier_add_reason(
            out,
            "[K3] %u decoded bits - short fixed code typical of keyfob",
            fv->pwm_decoded_count);
    }
    if(fv->entropy >= 0.80f) {
        rules++;
        classifier_add_reason(
            out, "[K4] entropy=%.3f - structured signal content", (double)fv->entropy);
    }

    if(!fv->pwm_params.found) {
        out->reason_count = 0;
        return false;
    }
    if(rules < 2) {
        out->reason_count = 0;
        return false;
    }

    if(fv->frequency == 315000000u) {
        classifier_add_hint(out, "315MHz -> North American car keyfob");
    } else {
        classifier_add_hint(out, "433.92MHz -> EU/Asian car keyfob / short-range remote");
    }

    out->label = BitrawLabelKeyfobRemote;
    out->confidence = rules >= 4 ? BitrawConfMedium : BitrawConfLow;
    return true;
}

/* ============================ WEATHER_STATION ========================== */

static bool classify_weather(const FeatureVector* fv, ClassificationResult* out) {
    if(fv->frequency != 433920000u) return false;
    if(fv->te_us < 150.0f || fv->te_us > 600.0f) return false;

    int rules = 0;
    if(fv->te_us >= 150.0f && fv->te_us <= 600.0f) {
        rules++;
        classifier_add_reason(
            out, "[W1] TE=%.0fus - slow timing typical of weather sensor OOK", (double)fv->te_us);
    }
    if(fv->seg_count >= 1 && fv->seg_count <= 3) {
        rules++;
        classifier_add_reason(
            out, "[W2] %u segment(s) - weather stations send 1-2 repeats", fv->seg_count);
    }
    if(fv->mean_inner_size >= 80.0f && fv->mean_inner_size <= 600.0f) {
        rules++;
        classifier_add_reason(
            out,
            "[W3] Mean inner size %.0f bits - matches weather payload",
            (double)fv->mean_inner_size);
    }
    if(fv->zero_ratio >= 0.40f && fv->zero_ratio <= 0.75f) {
        rules++;
        classifier_add_reason(
            out, "[W4] zero_ratio=%.1f%% - dense signal data",
            (double)(fv->zero_ratio * 100.0f));
    }
    if(fv->entropy >= 0.85f) {
        rules++;
        classifier_add_reason(
            out, "[W5] entropy=%.3f - high information content", (double)fv->entropy);
    }
    if(fv->pwm_params.found) {
        rules--;
        classifier_add_warning(
            out, "Clean PWM detected - weather usually uses Manchester or non-uniform OOK");
    }

    BitrawConfidence conf;
    if(rules >= 5) {
        conf = BitrawConfMedium;
    } else if(rules >= 3) {
        conf = BitrawConfLow;
    } else {
        out->reason_count = 0;
        out->warning_count = 0;
        return false;
    }

    if(fv->te_us >= 460.0f && fv->te_us <= 520.0f) {
        classifier_add_hint(out, "TE ~488us -> Oregon Scientific protocol");
    } else if(fv->te_us >= 180.0f && fv->te_us <= 220.0f) {
        classifier_add_hint(out, "TE ~200us -> AcuRite protocol");
    } else if(fv->te_us >= 300.0f && fv->te_us <= 370.0f) {
        classifier_add_hint(out, "TE ~336us -> generic 433MHz sensor (Lacrosse/similar)");
    }

    out->label = BitrawLabelWeatherStation;
    out->confidence = conf;
    return true;
}

/* ======================== UNKNOWN_STRUCTURED =========================== */

static bool classify_unknown_structured(const FeatureVector* fv, ClassificationResult* out) {
    int indicators = 0;
    if(fv->entropy >= 0.70f) indicators++;
    if(fv->total_inner_bits >= 30) indicators++;
    if(fv->zero_ratio >= 0.30f && fv->zero_ratio <= 0.90f) indicators++;
    if(fv->run_variety < 0.6f) indicators++;

    classifier_add_reason(
        out,
        "[U1] %s",
        indicators >= 3
            ? "Signal has structure but does not match any known device profile"
            : "Ambiguous: partial signal or unrecognised modulation");

    out->label = BitrawLabelUnknownStructured;
    out->confidence = BitrawConfLow;
    return true;
}

/* =============================== Pipeline ============================== */

void classifier_run(const FeatureVector* fv, ClassificationResult* out) {
    typedef bool (*ClassifierFn)(const FeatureVector*, ClassificationResult*);
    static const ClassifierFn pipeline[] = {
        classify_noise,
        classify_amr_meter,
        classify_tpms,
        classify_alarm_sensor,
        classify_shutter_blind,
        classify_doorbell,
        classify_outlet_switch,
        classify_garage,
        classify_keyfob,
        classify_weather,
        classify_unknown_structured,
    };
    for(size_t i = 0; i < sizeof(pipeline) / sizeof(pipeline[0]); i++) {
        reset_result(out);
        if(pipeline[i](fv, out)) return;
    }
    /* Should never reach here */
    reset_result(out);
    out->label = BitrawLabelUnknownStructured;
    out->confidence = BitrawConfLow;
    classifier_add_reason(out, "No classifier matched");
}
