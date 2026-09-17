#include "sr_test.h"

#include "sr_settings.h"
#include "sr_types.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

_Static_assert(SR_SETTINGS_TEXT_MAX == 160, "TEXT_MAX is the NUL-inclusive buffer");
_Static_assert(SR_SETTINGS_BAUD_CHOICES == 6, "UI baud table has 6 entries");
_Static_assert(SR_SETTINGS_SOURCE_CHOICES == 2, "UI source table has 2 entries");
_Static_assert(SR_SETTINGS_VERSION == 1, "settings file Version is 1");

/* -------------------------------------------------------------------------- */
/* A standalone oracle                                                        */
/*                                                                            */
/* Differences from the implementation (to avoid a tautological test):        */
/*   - Copies the whole buffer into its own NUL-terminated buffer first, then */
/*     uses strchr / strstr / strcmp / strtoul                                */
/*   - Baud: strspn validates all-digits, then strtoul -- not a byte-by-byte  */
/*     accumulation                                                           */
/*   - Key: a whole-string strcmp, not memcmp plus a literal length           */
/*   - Line: strchr slices on '\n', not the implementation's two-pointer scan */
/*     for != '\n'                                                            */
/* The implementation must never call these functions on the input buffer;    */
/* the oracle only operates on its own copy.                                  */
/* -------------------------------------------------------------------------- */

static const uint32_t kOracleBaud[6] = {
    9600u, 19200u, 38400u, 57600u, 115200u, 230400u
};

static void oracle_defaults(SrSettings* s) {
    s->baud = 115200u;
    s->source = SrSourceUnknown;
    s->sound = true;
    s->vibro = true;
    s->backlight_always = true;
    s->stealth = false;
    s->debug_rows = false;
}

static bool oracle_baud_ok(uint32_t v) {
    size_t i;
    for(i = 0; i < 6u; i++) {
        if(kOracleBaud[i] == v) {
            return true;
        }
    }
    return false;
}

static bool oracle_eq(const SrSettings* a, const SrSettings* b) {
    return a->baud == b->baud && a->source == b->source && a->sound == b->sound &&
           a->vibro == b->vibro && a->backlight_always == b->backlight_always &&
           a->stealth == b->stealth && a->debug_rows == b->debug_rows;
}

static bool oracle_is_default(const SrSettings* s) {
    SrSettings d;
    oracle_defaults(&d);
    return oracle_eq(s, &d);
}

static void oracle_apply_kv(
    SrSettings* out,
    SrSettingsParseStats* st,
    const char* key,
    const char* val) {
    if(strcmp(key, "Baud") == 0) {
        size_t n = strlen(val);
        if(n > 0u && strspn(val, "0123456789") == n) {
            unsigned long v;
            errno = 0;
            v = strtoul(val, NULL, 10);
            if(errno != ERANGE && v <= 0xFFFFFFFFul && oracle_baud_ok((uint32_t)v)) {
                out->baud = (uint32_t)v;
                if(st) {
                    st->keys_known++;
                }
                return;
            }
        }
        out->baud = 115200u;
        if(st) {
            st->values_invalid++;
        }
        return;
    }
    if(strcmp(key, "Source") == 0) {
        if(strcmp(val, "auto") == 0) {
            out->source = SrSourceUnknown;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "marauder") == 0) {
            out->source = SrSourceMarauder;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "ghostesp") == 0) {
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
    if(strcmp(key, "Sound") == 0) {
        if(strcmp(val, "0") == 0) {
            out->sound = false;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "1") == 0) {
            out->sound = true;
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
    if(strcmp(key, "Vibro") == 0) {
        if(strcmp(val, "0") == 0) {
            out->vibro = false;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "1") == 0) {
            out->vibro = true;
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
    if(strcmp(key, "Backlight") == 0) {
        if(strcmp(val, "0") == 0) {
            out->backlight_always = false;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "1") == 0) {
            out->backlight_always = true;
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
    if(strcmp(key, "Stealth") == 0) {
        if(strcmp(val, "0") == 0) {
            out->stealth = false;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "1") == 0) {
            out->stealth = true;
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
    if(strcmp(key, "Debug") == 0) {
        if(strcmp(val, "0") == 0) {
            out->debug_rows = false;
            if(st) {
                st->keys_known++;
            }
        } else if(strcmp(val, "1") == 0) {
            out->debug_rows = true;
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

/*
 * Lines are split with memchr (the implementation uses while != '\n'). An embedded NUL does not
 * terminate a line. The ": " separator is found with an inline for loop, over the already-sliced
 * line copy -- not the implementation's two-pointer scan over the raw buffer. key/value are
 * copied into their own C strings before strcmp / strtoul. A slice containing a NUL can never be
 * a legal token; if the key still matches a known name, the value is still treated as invalid.
 */
static bool oracle_parse(
    const char* buf,
    size_t len,
    SrSettings* out,
    SrSettingsParseStats* st) {
    const char* p;
    const char* end;
    unsigned phase;

    if(st) {
        memset(st, 0, sizeof(*st));
    }
    oracle_defaults(out);
    if(buf == NULL || len == 0u) {
        return false;
    }

    p = buf;
    end = buf + len;
    phase = 0;
    while(p < end) {
        const char* nl = (const char*)memchr(p, '\n', (size_t)(end - p));
        const char* line_end = (nl == NULL) ? end : nl;
        size_t L = (size_t)(line_end - p);
        char* line;
        const char* next = (nl == NULL) ? end : (nl + 1);

        if(L > 0u && p[L - 1u] == '\r') {
            L--;
        }
        if(L == 0u || p[0] == '#') {
            p = next;
            continue;
        }

        line = (char*)malloc(L + 1u);
        CHECK(line != NULL);
        if(line == NULL) {
            return false;
        }
        memcpy(line, p, L);
        line[L] = '\0';

        if(st) {
            st->lines_seen++;
        }
        if(phase == 0u) {
            if(L != sizeof("Filetype: SigRoam Settings") - 1u ||
               memchr(line, 0, L) != NULL ||
               strcmp(line, "Filetype: SigRoam Settings") != 0) {
                oracle_defaults(out);
                free(line);
                return false;
            }
            phase = 1u;
        } else if(phase == 1u) {
            if(L != sizeof("Version: 1") - 1u || memchr(line, 0, L) != NULL ||
               strcmp(line, "Version: 1") != 0) {
                oracle_defaults(out);
                free(line);
                return false;
            }
            phase = 2u;
            if(st) {
                st->header_ok = true;
            }
        } else {
            const char* sep = NULL;
            {
                size_t si;
                for(si = 0; si + 1u < L; si++) {
                    if(line[si] == ':' && line[si + 1u] == ' ') {
                        sep = line + si;
                        break;
                    }
                }
            }
            if(sep == NULL) {
                if(st) {
                    st->lines_malformed++;
                }
            } else {
                size_t klen = (size_t)(sep - line);
                size_t vlen = L - klen - 2u;
                char* key = (char*)malloc(klen + 1u);
                char* val = (char*)malloc(vlen + 1u);
                CHECK(key != NULL && val != NULL);
                if(key == NULL || val == NULL) {
                    free(key);
                    free(val);
                    free(line);
                    return false;
                }
                memcpy(key, line, klen);
                key[klen] = '\0';
                memcpy(val, sep + 2, vlen);
                val[vlen] = '\0';
                if(memchr(key, 0, klen) != NULL || memchr(val, 0, vlen) != NULL) {
                    /*
                     * A slice containing a NUL is not a legal token; if the key still matches a
                     * known name, the value is treated as invalid.
                     */
                    if(memchr(key, 0, klen) == NULL &&
                       (strcmp(key, "Baud") == 0 || strcmp(key, "Source") == 0 ||
                        strcmp(key, "Sound") == 0 || strcmp(key, "Vibro") == 0 ||
                        strcmp(key, "Backlight") == 0 || strcmp(key, "Stealth") == 0)) {
                        oracle_apply_kv(out, st, key, "\xff");
                    } else if(st) {
                        st->keys_unknown++;
                    }
                } else {
                    oracle_apply_kv(out, st, key, val);
                }
                free(key);
                free(val);
            }
        }
        free(line);
        p = next;
    }

    if(phase < 2u) {
        oracle_defaults(out);
        return false;
    }
    return true;
}

/*
 * Coverage counter: any recognized body key that appears >= 2 times. Written differently from
 * both parsers.
 */
static int sample_has_dup_key(const char* buf, size_t n) {
    char* tmp;
    char* p;
    int counts[6];
    unsigned content;

    if(buf == NULL || n == 0u) {
        return 0;
    }
    tmp = (char*)malloc(n + 1u);
    if(tmp == NULL) {
        return 0;
    }
    memcpy(tmp, buf, n);
    tmp[n] = '\0';
    memset(counts, 0, sizeof(counts));
    content = 0;
    p = tmp;
    while(p != NULL) {
        char* nl = strchr(p, '\n');
        size_t L;
        char* sep;
        if(nl) {
            *nl = '\0';
        }
        L = strlen(p);
        if(L > 0u && p[L - 1u] == '\r') {
            p[L - 1u] = '\0';
        }
        if(p[0] == '\0' || p[0] == '#') {
            p = nl ? (nl + 1) : NULL;
            continue;
        }
        content++;
        if(content <= 2u) {
            p = nl ? (nl + 1) : NULL;
            continue;
        }
        sep = strstr(p, ": ");
        if(sep) {
            *sep = '\0';
            if(strcmp(p, "Baud") == 0) {
                counts[0]++;
            } else if(strcmp(p, "Source") == 0) {
                counts[1]++;
            } else if(strcmp(p, "Sound") == 0) {
                counts[2]++;
            } else if(strcmp(p, "Vibro") == 0) {
                counts[3]++;
            } else if(strcmp(p, "Backlight") == 0) {
                counts[4]++;
            } else if(strcmp(p, "Stealth") == 0) {
                counts[5]++;
            }
        }
        p = nl ? (nl + 1) : NULL;
    }
    free(tmp);
    {
        int i;
        for(i = 0; i < 6; i++) {
            if(counts[i] >= 2) {
                return 1;
            }
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Basics: defaults / validation / clamping / baud table / NULL pointers      */
/* -------------------------------------------------------------------------- */

static void test_defaults_and_clamp(void) {
    SrSettings s;

    sr_settings_defaults(NULL);
    sr_settings_clamp(NULL);
    CHECK(!sr_settings_is_valid(NULL));
    CHECK(!sr_settings_effective_sound(NULL));
    CHECK(!sr_settings_effective_vibro(NULL));
    CHECK(!sr_settings_effective_backlight(NULL));
    CHECK(sr_settings_serialize(NULL, NULL, 0) == 0);

    sr_settings_defaults(&s);
    CHECK(s.baud == 115200u);
    CHECK(s.source == SrSourceUnknown);
    CHECK(s.sound == true);
    CHECK(s.vibro == true);
    CHECK(s.backlight_always == true);
    CHECK(s.stealth == false);
    CHECK(sr_settings_is_valid(&s));

    s.baud = 1u;
    s.source = SrSourceNative;
    CHECK(!sr_settings_is_valid(&s));
    sr_settings_clamp(&s);
    CHECK(s.baud == 115200u);
    CHECK(s.source == SrSourceUnknown);
    CHECK(sr_settings_is_valid(&s));

    CHECK(sr_settings_baud_choice(0) == 9600u);
    CHECK(sr_settings_baud_choice(1) == 19200u);
    CHECK(sr_settings_baud_choice(2) == 38400u);
    CHECK(sr_settings_baud_choice(3) == 57600u);
    CHECK(sr_settings_baud_choice(4) == 115200u);
    CHECK(sr_settings_baud_choice(5) == 230400u);
    CHECK(sr_settings_baud_choice(6) == 0u);
    CHECK(sr_settings_baud_choice(99) == 0u);
    CHECK(sr_settings_baud_index(9600u) == 0u);
    CHECK(sr_settings_baud_index(230400u) == 5u);
    CHECK(sr_settings_baud_index(115201u) == (size_t)SR_SETTINGS_BAUD_CHOICES);
}

/* -------------------------------------------------------------------------- */
/* Serialized length + cap boundary                                           */
/* -------------------------------------------------------------------------- */

static void fill_longest(SrSettings* s) {
    sr_settings_defaults(s);
    s->baud = 230400u;
    s->source = SrSourceGhostesp;
    s->sound = true;
    s->vibro = true;
    s->backlight_always = true;
    s->stealth = true;
}

static void test_serialize_len_and_cap(void) {
    SrSettings s;
    char buf[SR_SETTINGS_TEXT_MAX];
    char tight[119];
    size_t n;

    fill_longest(&s);
    memset(buf, 0xAA, sizeof(buf));
    n = sr_settings_serialize(&s, buf, sizeof(buf));
    printf("serialize longest = %zu\n", n);
    CHECK(n == 119u);
    CHECK(buf[n] == '\0');

    CHECK(sr_settings_serialize(&s, NULL, 0) == 0);

    memset(tight, 0xAA, sizeof(tight));
    n = sr_settings_serialize(&s, tight, 119u);
    CHECK(n == 0u);
    CHECK(tight[0] == '\0');

    memset(buf, 0xAA, sizeof(buf));
    n = sr_settings_serialize(&s, buf, 120u);
    CHECK(n == 119u);
    CHECK(buf[119] == '\0');

    /*
     * Lead-session review addition: serialize must never write out a file it cannot read back. An
     * illegal source (SrSourceNative, which has no adapter in phase one) falls back to auto per
     * clamp semantics. What gives this assertion its teeth: if source_token were serialized back
     * as "native", none of the three parse tokens would match -> values_invalid would become 1,
     * and the very next assertion below would go red (source would still end up Unknown, so that
     * alone would not catch it).
     */
    {
        SrSettings bad, back;
        SrSettingsParseStats st;

        sr_settings_defaults(&bad);
        bad.source = SrSourceNative;
        n = sr_settings_serialize(&bad, buf, sizeof(buf));
        CHECK(n > 0u);
        CHECK(sr_settings_parse(buf, n, &back, &st));
        CHECK(st.values_invalid == 0u);
        CHECK(st.keys_known == 7u);
        CHECK(back.source == SrSourceUnknown);
        CHECK(sr_settings_is_valid(&back));
    }
}

/* -------------------------------------------------------------------------- */
/* 576-combination round-trip (288 × 2, T4.11 added debug_rows)               */
/* -------------------------------------------------------------------------- */

static const SrSourceKind kSrc3[3] = {
    SrSourceUnknown, SrSourceMarauder, SrSourceGhostesp
};

static void test_roundtrip_576(void) {
    size_t bi, si, mask;
    unsigned n = 0;

    for(bi = 0; bi < 6u; bi++) {
        for(si = 0; si < 3u; si++) {
            for(mask = 0; mask < 32u; mask++) {
                SrSettings in, out;
                SrSettingsParseStats st;
                char text[SR_SETTINGS_TEXT_MAX];
                size_t w;
                bool ok;

                sr_settings_defaults(&in);
                in.baud = sr_settings_baud_choice(bi);
                in.source = kSrc3[si];
                in.sound = (mask & 1u) != 0u;
                in.vibro = (mask & 2u) != 0u;
                in.backlight_always = (mask & 4u) != 0u;
                in.stealth = (mask & 8u) != 0u;
                in.debug_rows = (mask & 16u) != 0u;

                w = sr_settings_serialize(&in, text, sizeof(text));
                CHECK(w > 0u);
                ok = sr_settings_parse(text, w, &out, &st);
                CHECK(ok);
                CHECK(st.header_ok);
                CHECK(st.keys_known == 7u);
                CHECK(st.keys_unknown == 0u);
                CHECK(st.values_invalid == 0u);
                CHECK(st.lines_malformed == 0u);
                CHECK(st.lines_seen == 9u);
                CHECK(out.baud == in.baud);
                CHECK(out.source == in.source);
                CHECK(out.sound == in.sound);
                CHECK(out.vibro == in.vibro);
                CHECK(out.backlight_always == in.backlight_always);
                CHECK(out.stealth == in.stealth);
                CHECK(out.debug_rows == in.debug_rows);
                CHECK(sr_settings_is_valid(&out));
                n++;
            }
        }
    }
    CHECK(n == 576u);
}

/* -------------------------------------------------------------------------- */
/* No NUL terminator (ASan's Achilles' heel)                                  */
/* -------------------------------------------------------------------------- */

static void parse_exact(const char* text, size_t n, SrSettings* out, SrSettingsParseStats* st) {
    char* p = NULL;
    if(n > 0u) {
        p = (char*)malloc(n);
        CHECK(p != NULL);
        memcpy(p, text, n);
    }
    (void)sr_settings_parse(p, n, out, st);
    CHECK(sr_settings_is_valid(out));
    free(p);
}

static void test_no_nul_term(void) {
    SrSettings s, out;
    SrSettingsParseStats st;
    char text[SR_SETTINGS_TEXT_MAX];
    size_t n;
    const char* cut_ft = "Filetype: SigRoam Settin";
    const char* cut_bau = "Bau";
    const char* cut_baud = "Baud: ";
    const char hash_only = '#';

    fill_longest(&s);
    n = sr_settings_serialize(&s, text, sizeof(text));
    CHECK(n == 119u);
    CHECK(text[n - 1u] == '\n');

    /* ① a complete, legal text with no trailing newline */
    parse_exact(text, n - 1u, &out, &st);
    CHECK(st.header_ok);
    CHECK(out.baud == 230400u);
    CHECK(out.source == SrSourceGhostesp);
    CHECK(out.stealth == true);

    /* ② cut off at Filetype: SigRoam Settin */
    parse_exact(cut_ft, strlen(cut_ft), &out, &st);
    CHECK(!st.header_ok);
    CHECK(oracle_is_default(&out));

    /* ③ cut off at Bau */
    parse_exact(cut_bau, strlen(cut_bau), &out, &st);
    CHECK(!st.header_ok);
    CHECK(oracle_is_default(&out));

    /* ④ cut off at Baud: (nothing left after the separator) */
    parse_exact(cut_baud, strlen(cut_baud), &out, &st);
    CHECK(!st.header_ok);
    CHECK(oracle_is_default(&out));

    /*
     * ④b header is complete + Baud: has an empty value with no trailing newline -- this actually
     * reaches the edge of the value scan.
     */
    {
        const char* empty_baud = "Filetype: SigRoam Settings\nVersion: 1\nBaud: ";
        parse_exact(empty_baud, strlen(empty_baud), &out, &st);
        CHECK(st.header_ok);
        CHECK(st.values_invalid == 1u);
        CHECK(out.baud == 115200u);
    }

    /* ⑤ just a single # */
    parse_exact(&hash_only, 1u, &out, &st);
    CHECK(!st.header_ok);
    CHECK(oracle_is_default(&out));

    /* ⑥ len == 0, buf passed as NULL */
    memset(&st, 0x5A, sizeof(st));
    sr_settings_defaults(&out);
    out.baud = 1;
    CHECK(!sr_settings_parse(NULL, 0, &out, &st));
    CHECK(!st.header_ok);
    CHECK(oracle_is_default(&out));
    CHECK(sr_settings_is_valid(&out));
}

/* -------------------------------------------------------------------------- */
/* Rule by rule                                                               */
/* -------------------------------------------------------------------------- */

static void test_rules(void) {
    SrSettings out;
    SrSettingsParseStats st;
    const char* s;
    bool ok;

    /* header mismatch */
    s = "Filetype: Other Settings\nVersion: 1\nBaud: 9600\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(!ok);
    CHECK(!st.header_ok);
    CHECK(st.lines_seen == 1u);
    CHECK(oracle_is_default(&out));

    /* Version: 2 */
    s = "Filetype: SigRoam Settings\nVersion: 2\nBaud: 9600\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(!ok);
    CHECK(!st.header_ok);
    CHECK(st.lines_seen == 2u);
    CHECK(oracle_is_default(&out));

    /* unknown key */
    s = "Filetype: SigRoam Settings\nVersion: 1\nFoo: bar\nBaud: 9600\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.header_ok);
    CHECK(st.keys_unknown == 1u);
    CHECK(st.keys_known == 1u);
    CHECK(out.baud == 9600u);

    /* illegal values: a baud rate not in the table / true / uppercase AUTO */
    s = "Filetype: SigRoam Settings\nVersion: 1\n"
        "Baud: 123\nSound: true\nSource: AUTO\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.values_invalid == 3u);
    CHECK(st.keys_known == 0u);
    CHECK(out.baud == 115200u);
    CHECK(out.sound == true);
    CHECK(out.source == SrSourceUnknown);

    /* duplicate key: the later one wins (both values legal) */
    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud: 9600\nBaud: 230400\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(out.baud == 230400u);
    CHECK(st.keys_known == 2u);
    CHECK(st.values_invalid == 0u);

    /* duplicate key: the later one is illegal -> the field falls back to its default (ADR-012) */
    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud: 9600\nBaud: 999\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(out.baud == 115200u);
    CHECK(st.keys_known == 1u);
    CHECK(st.values_invalid == 1u);

    /* # comments + blank lines interleaved with the header are still recognized as the header */
    s = "# comment\n\nFiletype: SigRoam Settings\n# mid\n\nVersion: 1\n"
        "# trail\nSound: 0\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.header_ok);
    CHECK(out.sound == false);
    CHECK(st.lines_seen == 3u);
    CHECK(st.keys_known == 1u);

    /* \r\n line endings */
    s = "Filetype: SigRoam Settings\r\nVersion: 1\r\nBaud: 38400\r\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.header_ok);
    CHECK(out.baud == 38400u);
    CHECK(st.keys_known == 1u);
    CHECK(st.lines_seen == 3u);

    /* Source: native must be rejected */
    s = "Filetype: SigRoam Settings\nVersion: 1\nSource: native\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.header_ok);
    CHECK(st.values_invalid == 1u);
    CHECK(st.keys_known == 0u);
    CHECK(out.source == SrSourceUnknown);

    /* a line with no ": " */
    s = "Filetype: SigRoam Settings\nVersion: 1\nnocolon\nBaud: 19200\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.lines_malformed == 1u);
    CHECK(st.keys_known == 1u);
    CHECK(out.baud == 19200u);

    /* Baud with surrounding whitespace / an empty value / overflow */
    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud:  115200\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.values_invalid == 1u);
    CHECK(out.baud == 115200u);

    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud: 115200 \n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.values_invalid == 1u);

    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud: \n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.values_invalid == 1u);

    s = "Filetype: SigRoam Settings\nVersion: 1\nBaud: 4294967296\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.values_invalid == 1u);
    CHECK(out.baud == 115200u);

    /* missing key: a header match alone is success, missing fields fall back to defaults */
    s = "Filetype: SigRoam Settings\nVersion: 1\n";
    ok = sr_settings_parse(s, strlen(s), &out, &st);
    CHECK(ok);
    CHECK(st.header_ok);
    CHECK(st.keys_known == 0u);
    CHECK(oracle_is_default(&out));
}

/* -------------------------------------------------------------------------- */
/* effective_* truth table, 16 combinations                                   */
/* -------------------------------------------------------------------------- */

static void test_effective_16(void) {
    unsigned mask;
    for(mask = 0; mask < 16u; mask++) {
        SrSettings s;
        bool stealth = (mask & 8u) != 0u;
        bool sound = (mask & 4u) != 0u;
        bool vibro = (mask & 2u) != 0u;
        bool bl = (mask & 1u) != 0u;

        sr_settings_defaults(&s);
        s.stealth = stealth;
        s.sound = sound;
        s.vibro = vibro;
        s.backlight_always = bl;

        if(stealth) {
            CHECK(sr_settings_effective_sound(&s) == false);
            CHECK(sr_settings_effective_vibro(&s) == false);
            CHECK(sr_settings_effective_backlight(&s) == false);
        } else {
            CHECK(sr_settings_effective_sound(&s) == sound);
            CHECK(sr_settings_effective_vibro(&s) == vibro);
            CHECK(sr_settings_effective_backlight(&s) == bl);
        }
        /* the stored values are not rewritten by the derived ones */
        CHECK(s.sound == sound);
        CHECK(s.vibro == vibro);
        CHECK(s.backlight_always == bl);
        CHECK(s.stealth == stealth);
    }
}

/* -------------------------------------------------------------------------- */
/* Fuzz                                                                       */
/* -------------------------------------------------------------------------- */

static uint32_t xs32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

enum { WORK_MAX = 512 };

static const char* kBadTok[] = {
    "yes",
    "true",
    "on",
    "2",
    "999",
    "native",
    "AUTO",
    " 1",
    "1 ",
    "",
    "4294967296",
    "115200x"
};
enum { N_BAD_TOK = 12 };

static size_t line_starts(const char* s, size_t n, size_t* out, size_t max) {
    size_t c = 0;
    size_t i;
    if(n == 0u || max == 0u) {
        return 0;
    }
    out[c++] = 0;
    for(i = 0; i + 1u < n && c < max; i++) {
        if(s[i] == '\n') {
            out[c++] = i + 1u;
        }
    }
    return c;
}

static size_t line_end(const char* s, size_t n, size_t start) {
    size_t i = start;
    while(i < n && s[i] != '\n') {
        i++;
    }
    if(i < n) {
        i++;
    }
    return i;
}

static void work_insert(char* w, size_t* n, size_t at, const char* ins, size_t ilen) {
    if(at > *n || *n + ilen > (size_t)WORK_MAX) {
        return;
    }
    memmove(w + at + ilen, w + at, *n - at);
    memcpy(w + at, ins, ilen);
    *n += ilen;
}

static void work_delete(char* w, size_t* n, size_t at, size_t dn) {
    if(at >= *n || dn == 0u) {
        return;
    }
    if(at + dn > *n) {
        dn = *n - at;
    }
    memmove(w + at, w + at + dn, *n - (at + dn));
    *n -= dn;
}

static void test_fuzz_a(void) {
    uint32_t seed = 0x51E771A1u;
    uint32_t i;
    uint32_t cov_valid = 0;
    uint32_t cov_default_on_fail = 0;
    uint32_t cov_hdr_ok = 0;

    for(i = 0; i < 20000u; i++) {
        uint32_t case_seed = seed;
        size_t n = (size_t)(xs32(&seed) % 301u);
        char* p = NULL;
        SrSettings out;
        SrSettingsParseStats st;
        bool ok;
        size_t j;

        if(n > 0u) {
            p = (char*)malloc(n);
            CHECK(p != NULL);
            for(j = 0; j < n; j++) {
                p[j] = (char)(xs32(&seed) & 0xFFu);
            }
        }
        ok = sr_settings_parse(p, n, &out, &st);
        if(!sr_settings_is_valid(&out)) {
            fprintf(
                stderr,
                "fuzz A invalid: iter=%u case_seed=0x%08X n=%zu\n",
                i,
                case_seed,
                n);
        }
        CHECK(sr_settings_is_valid(&out));
        cov_valid++;
        if(!ok) {
            if(!oracle_is_default(&out)) {
                fprintf(
                    stderr,
                    "fuzz A fail-not-default: iter=%u case_seed=0x%08X n=%zu\n",
                    i,
                    case_seed,
                    n);
            }
            CHECK(oracle_is_default(&out));
            CHECK(!st.header_ok);
            cov_default_on_fail++;
        } else {
            CHECK(st.header_ok);
            cov_hdr_ok++;
        }
        free(p);
    }
    printf(
        "settings fuzz A coverage: valid=%u fail_default=%u header_ok=%u\n",
        cov_valid,
        cov_default_on_fail,
        cov_hdr_ok);
    CHECK(cov_valid == 20000u);
    CHECK(cov_default_on_fail == 20000u);
}

static void test_fuzz_b(void) {
    uint32_t seed = 0xB012A001u;
    uint32_t i;
    uint32_t cov_hdr_ok = 0;
    uint32_t cov_hdr_fail = 0;
    uint32_t cov_unk = 0;
    uint32_t cov_inv = 0;
    uint32_t cov_mal = 0;
    uint32_t cov_dup = 0;
    uint32_t cov_trunc = 0;
    uint32_t cov_oracle = 0;

    for(i = 0; i < 20000u; i++) {
        uint32_t case_seed = seed;
        SrSettings base;
        char work[WORK_MAX];
        size_t n;
        uint32_t kind;
        char* p;
        SrSettings got, exp;
        SrSettingsParseStats gst, ost;
        bool gok, eok;

        sr_settings_defaults(&base);
        base.baud = sr_settings_baud_choice(xs32(&seed) % 6u);
        base.source = kSrc3[xs32(&seed) % 3u];
        base.sound = (xs32(&seed) & 1u) != 0u;
        base.vibro = (xs32(&seed) & 1u) != 0u;
        base.backlight_always = (xs32(&seed) & 1u) != 0u;
        base.stealth = (xs32(&seed) & 1u) != 0u;
        n = sr_settings_serialize(&base, work, sizeof(work));
        CHECK(n > 0u && n < (size_t)WORK_MAX);

        kind = xs32(&seed) % 10u;
        if((i % 20u) == 0u) {
            /* force the truncation path: cut right up against the dangerous boundaries */
            static const size_t kCuts[] = {0, 3, 8, 19, 24, 27, 38, 45};
            size_t cut = kCuts[xs32(&seed) % 8u];
            if(cut < n) {
                n = cut;
            } else if(n > 0u) {
                n = n / 2u;
            }
            kind = 2;
        } else if(kind == 0u || kind == 1u) {
            size_t k, reps = (size_t)(xs32(&seed) % 3u) + 1u;
            for(k = 0; k < reps && n > 0u; k++) {
                size_t at = (size_t)(xs32(&seed) % n);
                work[at] = (char)(xs32(&seed) & 0xFFu);
            }
        } else if(kind == 2u) {
            if(n > 0u) {
                n = (size_t)(xs32(&seed) % n);
            }
        } else if(kind == 3u) {
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            size_t li = (lc == 0u) ? 0u : (size_t)(xs32(&seed) % lc);
            const char* ins = "Foo: bar\n";
            work_insert(work, &n, (lc == 0u) ? 0u : starts[li], ins, 9u);
        } else if(kind == 4u) {
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            size_t li = (lc == 0u) ? 0u : (size_t)(xs32(&seed) % lc);
            const char* ins = "nocolon\n";
            work_insert(work, &n, (lc == 0u) ? 0u : starts[li], ins, 8u);
        } else if(kind == 5u) {
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            if(lc >= 3u) {
                size_t li = 2u + (size_t)(xs32(&seed) % (lc - 2u));
                size_t a = starts[li];
                size_t b = line_end(work, n, a);
                size_t ln = b - a;
                if(n + ln <= (size_t)WORK_MAX && ln > 0u) {
                    char hold[128];
                    if(ln <= sizeof(hold)) {
                        memcpy(hold, work + a, ln);
                        work_insert(work, &n, b, hold, ln);
                    }
                }
            }
        } else if(kind == 6u) {
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            if(lc >= 3u) {
                size_t li = 2u + (size_t)(xs32(&seed) % (lc - 2u));
                size_t a = starts[li];
                size_t b = line_end(work, n, a);
                size_t k;
                for(k = a; k + 1u < b; k++) {
                    if(work[k] == ':' && work[k + 1u] == ' ') {
                        const char* tok = kBadTok[xs32(&seed) % (uint32_t)N_BAD_TOK];
                        size_t tl = strlen(tok);
                        size_t val_a = k + 2u;
                        size_t val_b = b;
                        if(val_b > val_a && work[val_b - 1u] == '\n') {
                            val_b--;
                        }
                        if(val_b > val_a && work[val_b - 1u] == '\r') {
                            val_b--;
                        }
                        work_delete(work, &n, val_a, val_b - val_a);
                        work_insert(work, &n, val_a, tok, tl);
                        break;
                    }
                }
            }
        } else if(kind == 7u) {
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            size_t li = (lc == 0u) ? 0u : (size_t)(xs32(&seed) % lc);
            if((xs32(&seed) & 1u) != 0u) {
                work_insert(work, &n, (lc == 0u) ? 0u : starts[li], "# x\n", 4u);
            } else {
                work_insert(work, &n, (lc == 0u) ? 0u : starts[li], "\n", 1u);
            }
        } else if(kind == 8u) {
            /* delete a line */
            size_t starts[32];
            size_t lc = line_starts(work, n, starts, 32);
            if(lc > 0u) {
                size_t li = (size_t)(xs32(&seed) % lc);
                size_t a = starts[li];
                size_t b = line_end(work, n, a);
                work_delete(work, &n, a, b - a);
            }
        } else {
            /* turn some \n into \r\n */
            size_t k;
            for(k = 0; k < n; k++) {
                if(work[k] == '\n' && (xs32(&seed) % 3u) == 0u) {
                    work_insert(work, &n, k, "\r", 1u);
                    break;
                }
            }
        }

        if(kind == 2u) {
            cov_trunc++;
        }
        if(sample_has_dup_key(work, n)) {
            cov_dup++;
        }

        p = NULL;
        if(n > 0u) {
            p = (char*)malloc(n);
            CHECK(p != NULL);
            memcpy(p, work, n);
        }
        gok = sr_settings_parse(p, n, &got, &gst);
        eok = oracle_parse(p, n, &exp, &ost);
        free(p);

        if(!sr_settings_is_valid(&got)) {
            fprintf(
                stderr,
                "fuzz B invalid: iter=%u case_seed=0x%08X n=%zu kind=%u\n",
                i,
                case_seed,
                n,
                kind);
        }
        CHECK(sr_settings_is_valid(&got));
        if(!gok) {
            CHECK(oracle_is_default(&got));
            CHECK(!gst.header_ok);
            cov_hdr_fail++;
        } else {
            CHECK(gst.header_ok);
            cov_hdr_ok++;
            if(gok != eok || !oracle_eq(&got, &exp)) {
                fprintf(
                    stderr,
                    "fuzz B oracle mismatch: iter=%u case_seed=0x%08X n=%zu "
                    "kind=%u gok=%d eok=%d baud %u/%u src %d/%d\n",
                    i,
                    case_seed,
                    n,
                    kind,
                    (int)gok,
                    (int)eok,
                    got.baud,
                    exp.baud,
                    (int)got.source,
                    (int)exp.source);
            }
            CHECK(gok == eok);
            CHECK(oracle_eq(&got, &exp));
            CHECK(gst.header_ok == ost.header_ok);
            CHECK(gst.keys_known == ost.keys_known);
            CHECK(gst.keys_unknown == ost.keys_unknown);
            CHECK(gst.values_invalid == ost.values_invalid);
            CHECK(gst.lines_malformed == ost.lines_malformed);
            cov_oracle++;
        }
        if(gst.keys_unknown > 0u) {
            cov_unk++;
        }
        if(gst.values_invalid > 0u) {
            cov_inv++;
        }
        if(gst.lines_malformed > 0u) {
            cov_mal++;
        }
    }

    printf(
        "settings fuzz B coverage: header_ok=%u header_fail=%u unk=%u inv=%u "
        "mal=%u dup=%u trunc=%u oracle=%u\n",
        cov_hdr_ok,
        cov_hdr_fail,
        cov_unk,
        cov_inv,
        cov_mal,
        cov_dup,
        cov_trunc,
        cov_oracle);
    /* First run measured (seed 0xB012A001): ok=14993 fail=5007 unk=2470 inv=2979
     * mal=2593 dup=1919 trunc=2910 oracle=14993. Floors are ~1/3 of that, counted separately
     * per dimension. */
    CHECK(cov_hdr_ok > 5000u);
    CHECK(cov_hdr_fail > 1600u);
    CHECK(cov_unk > 800u);
    CHECK(cov_inv > 900u);
    CHECK(cov_mal > 800u);
    CHECK(cov_dup > 600u);
    CHECK(cov_trunc > 900u);
    CHECK(cov_oracle == cov_hdr_ok);
    CHECK(cov_oracle > 5000u);
}

static void test_source_choice_index_equal(void) {
    size_t i;
    SrSettings a;
    SrSettings b;

    CHECK(sr_settings_source_choice(0) == SrSourceUnknown);
    CHECK(sr_settings_source_choice(1) == SrSourceMarauder);
    CHECK(sr_settings_source_choice(2) == SrSourceUnknown);
    CHECK(sr_settings_source_choice(3) == SrSourceUnknown);

    CHECK(sr_settings_source_index(SrSourceUnknown) == 0u);
    CHECK(sr_settings_source_index(SrSourceMarauder) == 1u);
    CHECK(sr_settings_source_index(SrSourceGhostesp) == (size_t)SR_SETTINGS_SOURCE_CHOICES);
    CHECK(sr_settings_source_index(SrSourceNative) == (size_t)SR_SETTINGS_SOURCE_CHOICES);

    for(i = 0; i < (size_t)SR_SETTINGS_SOURCE_CHOICES; i++) {
        CHECK(sr_settings_source_index(sr_settings_source_choice(i)) == i);
    }

    sr_settings_defaults(&a);
    sr_settings_defaults(&b);
    CHECK(sr_settings_equal(&a, &b));
    CHECK(sr_settings_equal(NULL, NULL));
    CHECK(!sr_settings_equal(&a, NULL));
    CHECK(!sr_settings_equal(NULL, &b));

    b.baud = 9600u;
    CHECK(!sr_settings_equal(&a, &b));
    b.baud = a.baud;

    b.source = SrSourceMarauder;
    CHECK(!sr_settings_equal(&a, &b));
    b.source = a.source;

    b.sound = false;
    CHECK(!sr_settings_equal(&a, &b));
    b.sound = a.sound;

    b.vibro = false;
    CHECK(!sr_settings_equal(&a, &b));
    b.vibro = a.vibro;

    b.backlight_always = false;
    CHECK(!sr_settings_equal(&a, &b));
    b.backlight_always = a.backlight_always;

    b.stealth = true;
    CHECK(!sr_settings_equal(&a, &b));
}

int test_settings_run(void) {
    test_defaults_and_clamp();
    test_serialize_len_and_cap();
    test_roundtrip_576();
    test_no_nul_term();
    test_rules();
    test_effective_16();
    test_fuzz_a();
    test_fuzz_b();
    test_source_choice_index_equal();
    return sr_test_failures;
}
