#include "sr_settings.h"

#include <stdio.h>
#include <string.h>

/*
 * ★ Pure logic; must not include furi (ADR-003 / ADR-012).
 * The baud rate table is a list of UI choices in a fixed order. It is not the SDK's support list.
 */
static const uint32_t kBaudChoices[SR_SETTINGS_BAUD_CHOICES] = {
    9600u,
    19200u,
    38400u,
    57600u,
    115200u,
    230400u
};

/* UI choices: Auto / Marauder. GhostESP was CUT and Native is illegal, so neither enters the table. */
static const SrSourceKind kSourceChoices[SR_SETTINGS_SOURCE_CHOICES] = {
    SrSourceUnknown,
    SrSourceMarauder
};

static const char kHdrFiletype[] = "Filetype: SigRoam Settings";
static const char kHdrVersion[] = "Version: 1";

#define SR_BAUD_DEFAULT 115200u

static bool source_legal(SrSourceKind k) {
    return k == SrSourceUnknown || k == SrSourceMarauder || k == SrSourceGhostesp;
}

static const char* source_token(SrSourceKind k) {
    if(k == SrSourceMarauder) {
        return "marauder";
    }
    if(k == SrSourceGhostesp) {
        return "ghostesp";
    }
    /*
     * SrSourceUnknown and any illegal value (such as SrSourceNative) are all written as auto.
     * This matches the semantics of sr_settings_clamp and guarantees a serialized file always
     * parses back -- writing "native" would produce a file it cannot read back (parse would not
     * recognize it -> values_invalid).
     * Fixed during the main session's T1.2a review; the regression test is at the end of
     * test_serialize_len_and_cap.
     */
    return "auto";
}

static bool slice_eq(const char* p, size_t n, const char* lit, size_t lit_n) {
    return n == lit_n && memcmp(p, lit, lit_n) == 0;
}

static bool baud_in_table(uint32_t baud) {
    size_t i;
    for(i = 0; i < (size_t)SR_SETTINGS_BAUD_CHOICES; i++) {
        if(kBaudChoices[i] == baud) {
            return true;
        }
    }
    return false;
}

/* Pure decimal, no whitespace, non-empty, no uint32_t overflow. On failure *out is not written. */
static bool parse_u32_strict(const char* p, size_t n, uint32_t* out) {
    size_t i;
    uint32_t v = 0;

    if(p == NULL || out == NULL || n == 0) {
        return false;
    }
    for(i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        uint32_t d;
        if(c < (unsigned char)'0' || c > (unsigned char)'9') {
            return false;
        }
        d = (uint32_t)(c - (unsigned char)'0');
        if(v > (0xFFFFFFFFu / 10u)) {
            return false;
        }
        v *= 10u;
        if(v > (0xFFFFFFFFu - d)) {
            return false;
        }
        v += d;
    }
    *out = v;
    return true;
}

static bool parse_bit(const char* p, size_t n, bool* out) {
    if(p == NULL || out == NULL || n != 1u) {
        return false;
    }
    if(p[0] == '0') {
        *out = false;
        return true;
    }
    if(p[0] == '1') {
        *out = true;
        return true;
    }
    return false;
}

static void stats_zero(SrSettingsParseStats* st) {
    if(st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
}

static void apply_kv(
    SrSettings* out,
    SrSettingsParseStats* st,
    const char* key,
    size_t klen,
    const char* val,
    size_t vlen) {
    uint32_t baud;
    bool bit;

    if(slice_eq(key, klen, "Baud", sizeof("Baud") - 1u)) {
        if(parse_u32_strict(val, vlen, &baud) && baud_in_table(baud)) {
            out->baud = baud;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->baud = SR_BAUD_DEFAULT;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Source", sizeof("Source") - 1u)) {
        if(slice_eq(val, vlen, "auto", sizeof("auto") - 1u)) {
            out->source = SrSourceUnknown;
            if(st) {
                st->keys_known++;
            }
        } else if(slice_eq(val, vlen, "marauder", sizeof("marauder") - 1u)) {
            out->source = SrSourceMarauder;
            if(st) {
                st->keys_known++;
            }
        } else if(slice_eq(val, vlen, "ghostesp", sizeof("ghostesp") - 1u)) {
            out->source = SrSourceGhostesp;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->source = SrSourceUnknown;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Sound", sizeof("Sound") - 1u)) {
        if(parse_bit(val, vlen, &bit)) {
            out->sound = bit;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->sound = true;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Vibro", sizeof("Vibro") - 1u)) {
        if(parse_bit(val, vlen, &bit)) {
            out->vibro = bit;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->vibro = true;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Backlight", sizeof("Backlight") - 1u)) {
        if(parse_bit(val, vlen, &bit)) {
            out->backlight_always = bit;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->backlight_always = true;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Stealth", sizeof("Stealth") - 1u)) {
        if(parse_bit(val, vlen, &bit)) {
            out->stealth = bit;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->stealth = false;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(slice_eq(key, klen, "Debug", sizeof("Debug") - 1u)) {
        if(parse_bit(val, vlen, &bit)) {
            out->debug_rows = bit;
            if(st) {
                st->keys_known++;
            }
        } else {
            out->debug_rows = false;
            if(st) {
                st->values_invalid++;
            }
        }
        return;
    }
    if(st) {
        st->keys_unknown++;
    }
}

void sr_settings_defaults(SrSettings* out) {
    if(out == NULL) {
        return;
    }
    out->baud = SR_BAUD_DEFAULT;
    out->source = SrSourceUnknown;
    out->sound = true;
    out->vibro = true;
    out->backlight_always = true;
    out->stealth = false;
    out->debug_rows = false;
}

bool sr_settings_is_valid(const SrSettings* s) {
    if(s == NULL) {
        return false;
    }
    if(!baud_in_table(s->baud)) {
        return false;
    }
    if(!source_legal(s->source)) {
        return false;
    }
    return true;
}

void sr_settings_clamp(SrSettings* s) {
    if(s == NULL) {
        return;
    }
    if(!baud_in_table(s->baud)) {
        s->baud = SR_BAUD_DEFAULT;
    }
    if(!source_legal(s->source)) {
        s->source = SrSourceUnknown;
    }
}

size_t sr_settings_serialize(const SrSettings* s, char* out, size_t cap) {
    const char* src;
    int n;

    if(s == NULL) {
        if(out != NULL && cap > 0u) {
            out[0] = '\0';
        }
        return 0;
    }

    src = source_token(s->source);
    n = snprintf(
        NULL,
        0,
        "Filetype: SigRoam Settings\n"
        "Version: 1\n"
        "Baud: %u\n"
        "Source: %s\n"
        "Sound: %u\n"
        "Vibro: %u\n"
        "Backlight: %u\n"
        "Stealth: %u\n"
        "Debug: %u\n",
        (unsigned)s->baud,
        src,
        s->sound ? 1u : 0u,
        s->vibro ? 1u : 0u,
        s->backlight_always ? 1u : 0u,
        s->stealth ? 1u : 0u,
        s->debug_rows ? 1u : 0u);
    if(n < 0 || cap == 0u || (size_t)n + 1u > cap) {
        if(out != NULL && cap > 0u) {
            out[0] = '\0';
        }
        return 0;
    }
    snprintf(
        out,
        cap,
        "Filetype: SigRoam Settings\n"
        "Version: 1\n"
        "Baud: %u\n"
        "Source: %s\n"
        "Sound: %u\n"
        "Vibro: %u\n"
        "Backlight: %u\n"
        "Stealth: %u\n"
        "Debug: %u\n",
        (unsigned)s->baud,
        src,
        s->sound ? 1u : 0u,
        s->vibro ? 1u : 0u,
        s->backlight_always ? 1u : 0u,
        s->stealth ? 1u : 0u,
        s->debug_rows ? 1u : 0u);
    return (size_t)n;
}

bool sr_settings_parse(
    const char* buf,
    size_t len,
    SrSettings* out,
    SrSettingsParseStats* stats) {
    size_t i;
    unsigned header_got;

    stats_zero(stats);
    if(out == NULL) {
        return false;
    }
    sr_settings_defaults(out);

    if(buf == NULL || len == 0u) {
        return false;
    }

    i = 0;
    header_got = 0;
    while(i < len) {
        size_t start = i;
        size_t end;
        size_t k;

        while(i < len && buf[i] != '\n') {
            i++;
        }
        end = i;
        if(i < len) {
            i++;
        }
        if(end > start && buf[end - 1u] == '\r') {
            end--;
        }
        if(end == start) {
            continue;
        }
        if(buf[start] == '#') {
            continue;
        }

        if(stats) {
            stats->lines_seen++;
        }

        if(header_got == 0u) {
            if(!slice_eq(
                   buf + start,
                   end - start,
                   kHdrFiletype,
                   sizeof(kHdrFiletype) - 1u)) {
                sr_settings_defaults(out);
                return false;
            }
            header_got = 1u;
            continue;
        }
        if(header_got == 1u) {
            if(!slice_eq(
                   buf + start,
                   end - start,
                   kHdrVersion,
                   sizeof(kHdrVersion) - 1u)) {
                sr_settings_defaults(out);
                return false;
            }
            header_got = 2u;
            if(stats) {
                stats->header_ok = true;
            }
            continue;
        }

        {
            size_t sep = end;
            for(k = start; k + 1u < end; k++) {
                if(buf[k] == ':' && buf[k + 1u] == ' ') {
                    sep = k;
                    break;
                }
            }
            if(sep == end) {
                if(stats) {
                    stats->lines_malformed++;
                }
                continue;
            }
            apply_kv(
                out,
                stats,
                buf + start,
                sep - start,
                buf + sep + 2u,
                end - (sep + 2u));
        }
    }

    if(header_got < 2u) {
        sr_settings_defaults(out);
        return false;
    }
    return true;
}

bool sr_settings_effective_sound(const SrSettings* s) {
    if(s == NULL) {
        return false;
    }
    if(s->stealth) {
        return false;
    }
    return s->sound;
}

bool sr_settings_effective_vibro(const SrSettings* s) {
    if(s == NULL) {
        return false;
    }
    if(s->stealth) {
        return false;
    }
    return s->vibro;
}

bool sr_settings_effective_backlight(const SrSettings* s) {
    if(s == NULL) {
        return false;
    }
    if(s->stealth) {
        return false;
    }
    return s->backlight_always;
}

uint32_t sr_settings_baud_choice(size_t idx) {
    if(idx >= (size_t)SR_SETTINGS_BAUD_CHOICES) {
        return 0;
    }
    return kBaudChoices[idx];
}

size_t sr_settings_baud_index(uint32_t baud) {
    size_t i;
    for(i = 0; i < (size_t)SR_SETTINGS_BAUD_CHOICES; i++) {
        if(kBaudChoices[i] == baud) {
            return i;
        }
    }
    return (size_t)SR_SETTINGS_BAUD_CHOICES;
}

SrSourceKind sr_settings_source_choice(size_t idx) {
    if(idx >= (size_t)SR_SETTINGS_SOURCE_CHOICES) {
        return SrSourceUnknown;
    }
    return kSourceChoices[idx];
}

size_t sr_settings_source_index(SrSourceKind k) {
    size_t i;
    for(i = 0; i < (size_t)SR_SETTINGS_SOURCE_CHOICES; i++) {
        if(kSourceChoices[i] == k) {
            return i;
        }
    }
    return (size_t)SR_SETTINGS_SOURCE_CHOICES;
}

bool sr_settings_equal(const SrSettings* a, const SrSettings* b) {
    if(a == NULL && b == NULL) {
        return true;
    }
    if(a == NULL || b == NULL) {
        return false;
    }
    return a->baud == b->baud && a->source == b->source && a->sound == b->sound &&
           a->vibro == b->vibro && a->backlight_always == b->backlight_always &&
           a->stealth == b->stealth && a->debug_rows == b->debug_rows;
}
