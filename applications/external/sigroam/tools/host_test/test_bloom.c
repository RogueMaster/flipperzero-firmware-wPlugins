#include "sr_test.h"

#include "sr_bloom.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

_Static_assert(sizeof(SrBloom) <= 4108, "SrBloom over 4 KB + counter budget");
_Static_assert(SR_BLOOM_BITS == 32768, "m is Plan 3.5 4 KB budget");
_Static_assert(SR_BLOOM_BYTES == 4096, "m/8");
_Static_assert(SR_BLOOM_K == 4, "k is the T2.5 board decision");

/* The 4 KB bit array is kept off the test stack. */
static SrBloom g_bloom;
static SrBloom g_bloom_b;

/* -------------------------------------------------------------------------- */
/* A standalone oracle: a uint64 bitmap, a lookup-table fold, an index loop   */
/* over FNV, and an accumulating double hash.                                 */
/* Deliberately written differently from sr_bloom.c's byte-pointer walk /     */
/* uint8 bitmap / i*h2 style.                                                 */
/* -------------------------------------------------------------------------- */

enum {
    ORACLE_WORDS = SR_BLOOM_BITS / 64,
    ORACLE_FNV_OFFSET = 2166136261u,
    ORACLE_FNV_PRIME = 16777619u
};

typedef struct {
    uint64_t words[ORACLE_WORDS];
    uint32_t added;
    uint32_t unique;
} OracleBloom;

static unsigned char g_fold[256];
static int g_fold_ready;

static void fold_init(void) {
    unsigned i;
    if(g_fold_ready) {
        return;
    }
    for(i = 0; i < 256u; i++) {
        g_fold[i] = (unsigned char)i;
    }
    g_fold[(unsigned char)'a'] = 'A';
    g_fold[(unsigned char)'b'] = 'B';
    g_fold[(unsigned char)'c'] = 'C';
    g_fold[(unsigned char)'d'] = 'D';
    g_fold[(unsigned char)'e'] = 'E';
    g_fold[(unsigned char)'f'] = 'F';
    g_fold_ready = 1;
}

static uint32_t oracle_fnv1a(const char* s, uint32_t basis) {
    uint32_t h = basis;
    size_t i;

    fold_init();
    if(s == NULL) {
        return h;
    }
    for(i = 0; s[i] != '\0'; i++) {
        uint32_t mixed = h ^ (uint32_t)g_fold[(unsigned char)s[i]];
        h = mixed * ORACLE_FNV_PRIME;
    }
    return h;
}

static void oracle_indices(const char* s, uint32_t out[SR_BLOOM_K]) {
    uint32_t h1 = oracle_fnv1a(s, ORACLE_FNV_OFFSET);
    uint32_t h2 = oracle_fnv1a(s, h1);
    unsigned i;
    uint32_t acc;

    h2 |= 1u;
    acc = h1;
    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        if(i != 0) {
            acc += h2; /* accumulate step by step, not i*h2 */
        }
        out[i] = acc & (uint32_t)(SR_BLOOM_BITS - 1);
    }
}

static void oracle_init(OracleBloom* o) {
    memset(o, 0, sizeof(*o));
}

static bool oracle_test_bit(const OracleBloom* o, uint32_t idx) {
    return (o->words[idx >> 6] & ((uint64_t)1 << (idx & 63u))) != 0;
}

static void oracle_set_bit(OracleBloom* o, uint32_t idx) {
    o->words[idx >> 6] |= (uint64_t)1 << (idx & 63u);
}

static bool oracle_add(OracleBloom* o, const char* bssid) {
    uint32_t idx[SR_BLOOM_K];
    unsigned i;
    bool fresh;

    if(o == NULL || bssid == NULL || bssid[0] == '\0') {
        return false;
    }
    oracle_indices(bssid, idx);
    fresh = false;
    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        if(!oracle_test_bit(o, idx[i])) {
            fresh = true;
        }
        oracle_set_bit(o, idx[i]);
    }
    o->added++;
    if(fresh) {
        o->unique++;
        return true;
    }
    return false;
}

static bool oracle_contains(const OracleBloom* o, const char* bssid) {
    uint32_t idx[SR_BLOOM_K];
    unsigned i;

    if(o == NULL || bssid == NULL || bssid[0] == '\0') {
        return false;
    }
    oracle_indices(bssid, idx);
    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        if(!oracle_test_bit(o, idx[i])) {
            return false;
        }
    }
    return true;
}

static uint32_t oracle_fill_bits(const OracleBloom* o) {
    uint32_t n = 0;
    size_t w;

    for(w = 0; w < (size_t)ORACLE_WORDS; w++) {
        uint64_t v = o->words[w];
        while(v != 0) {
            n += (uint32_t)(v & 1u);
            v >>= 1;
        }
    }
    return n;
}

/* -------------------------------------------------------------------------- */
/* MAC generation: the insert set uses prefix 0x02, the query set uses prefix */
/* 0x06; the two sets never overlap.                                          */
/* -------------------------------------------------------------------------- */

static const char kHexU[] = "0123456789ABCDEF";
static const char kHexL[] = "0123456789abcdef";

static void mac_bytes(uint8_t out[6], uint8_t prefix, uint32_t id) {
    out[0] = prefix;
    out[1] = (uint8_t)(id >> 24);
    out[2] = (uint8_t)(id >> 16);
    out[3] = (uint8_t)(id >> 8);
    out[4] = (uint8_t)id;
    out[5] = (uint8_t)(id * 0x9Eu + prefix);
}

static void mac_fmt(char* dst, const uint8_t b[6], const char* hex) {
    size_t i;
    size_t p = 0;

    for(i = 0; i < 6; i++) {
        if(i != 0) {
            dst[p++] = ':';
        }
        dst[p++] = hex[b[i] >> 4];
        dst[p++] = hex[b[i] & 0x0Fu];
    }
    dst[p] = '\0';
}

static void mac_id(char* dst, uint8_t prefix, uint32_t id) {
    uint8_t b[6];
    mac_bytes(b, prefix, id);
    mac_fmt(dst, b, kHexU);
}

/* -------------------------------------------------------------------------- */
/* Targeted test cases                                                        */
/* -------------------------------------------------------------------------- */

static void test_sizeof_and_init(void) {
    fprintf(stderr, "sizeof(SrBloom)=%zu\n", sizeof(SrBloom));
    CHECK(sizeof(SrBloom) <= 4108);
    CHECK(sizeof(g_bloom.bits) == (size_t)SR_BLOOM_BYTES);

    sr_bloom_init(&g_bloom);
    CHECK(sr_bloom_unique(&g_bloom) == 0);
    CHECK(sr_bloom_fill_bits(&g_bloom) == 0);
    CHECK(g_bloom.added == 0);
    CHECK(sr_bloom_maybe_contains(&g_bloom, "AA:BB:CC:DD:EE:FF") == false);
}

static void test_null_and_empty(void) {
    sr_bloom_init(&g_bloom);
    sr_bloom_init(NULL);

    CHECK(sr_bloom_add(NULL, "AA:BB:CC:DD:EE:FF") == false);
    CHECK(sr_bloom_add(&g_bloom, NULL) == false);
    CHECK(sr_bloom_add(&g_bloom, "") == false);
    CHECK(g_bloom.added == 0);
    CHECK(sr_bloom_unique(&g_bloom) == 0);
    CHECK(sr_bloom_fill_bits(&g_bloom) == 0);

    CHECK(sr_bloom_maybe_contains(NULL, "AA:BB:CC:DD:EE:FF") == false);
    CHECK(sr_bloom_maybe_contains(&g_bloom, NULL) == false);
    CHECK(sr_bloom_maybe_contains(&g_bloom, "") == false);
    CHECK(sr_bloom_unique(NULL) == 0);
    CHECK(sr_bloom_fill_bits(NULL) == 0);
}

static void test_case_fold(void) {
    sr_bloom_init(&g_bloom);

    CHECK(sr_bloom_add(&g_bloom, "aa:bb:cc:dd:ee:ff") == true);
    CHECK(sr_bloom_unique(&g_bloom) == 1);
    CHECK(g_bloom.added == 1);

    CHECK(sr_bloom_add(&g_bloom, "AA:BB:CC:DD:EE:FF") == false);
    CHECK(sr_bloom_unique(&g_bloom) == 1);
    CHECK(g_bloom.added == 2);

    CHECK(sr_bloom_maybe_contains(&g_bloom, "Aa:Bb:Cc:Dd:Ee:Ff") == true);
    CHECK(sr_bloom_maybe_contains(&g_bloom, "aa:bb:cc:dd:ee:ff") == true);
    CHECK(sr_bloom_maybe_contains(&g_bloom, "AA:BB:CC:DD:EE:FF") == true);

    /* Only a-f are folded; g and G must still count as two distinct keys */
    sr_bloom_init(&g_bloom);
    CHECK(sr_bloom_add(&g_bloom, "02:00:00:00:00:0g") == true);
    CHECK(sr_bloom_add(&g_bloom, "02:00:00:00:00:0G") == true);
    CHECK(sr_bloom_unique(&g_bloom) == 2);
}

static void test_duplicate_and_reinit(void) {
    sr_bloom_init(&g_bloom);
    CHECK(sr_bloom_add(&g_bloom, "02:00:00:00:00:01") == true);
    CHECK(sr_bloom_add(&g_bloom, "02:00:00:00:00:01") == false);
    CHECK(sr_bloom_unique(&g_bloom) == 1);
    CHECK(g_bloom.added == 2);
    CHECK(sr_bloom_fill_bits(&g_bloom) == (uint32_t)SR_BLOOM_K);

    sr_bloom_init(&g_bloom);
    CHECK(sr_bloom_unique(&g_bloom) == 0);
    CHECK(sr_bloom_fill_bits(&g_bloom) == 0);
    CHECK(g_bloom.added == 0);
}

static uint32_t raw_fnv1a(const char* s) {
    /* No case folding here; used only to check against the official test vectors. */
    uint32_t h = ORACLE_FNV_OFFSET;
    size_t i;
    for(i = 0; s[i] != '\0'; i++) {
        h ^= (uint32_t)(unsigned char)s[i];
        h *= ORACLE_FNV_PRIME;
    }
    return h;
}

static void test_fnv_vectors(void) {
    /* Official FNV-1a 32 test vectors. Source: Landon Curt Noll / Go hash/fnv, 2026-08-16 */
    CHECK(raw_fnv1a("") == 0x811C9DC5u);
    CHECK(raw_fnv1a("a") == 0xE40C292Cu);
    CHECK(raw_fnv1a("foobar") == 0xBF9CF968u);
    /* After folding, aa:bb... must equal AA:BB... */
    CHECK(oracle_fnv1a("aa:bb:cc:dd:ee:ff", ORACLE_FNV_OFFSET) ==
          oracle_fnv1a("AA:BB:CC:DD:EE:FF", ORACLE_FNV_OFFSET));
}

static void test_oracle_agreement(void) {
    static OracleBloom o;
    char mac[18];
    uint32_t i;

    sr_bloom_init(&g_bloom);
    oracle_init(&o);

    for(i = 0; i < 1500u; i++) {
        bool a;
        bool b;

        mac_id(mac, 0x02, i);
        a = sr_bloom_add(&g_bloom, mac);
        b = oracle_add(&o, mac);
        CHECK(a == b);
        CHECK(sr_bloom_unique(&g_bloom) == o.unique);
        CHECK(g_bloom.added == o.added);
        CHECK(sr_bloom_maybe_contains(&g_bloom, mac) == true);
        CHECK(oracle_contains(&o, mac) == true);
    }

    CHECK(sr_bloom_fill_bits(&g_bloom) == oracle_fill_bits(&o));

    for(i = 0; i < 1500u; i++) {
        mac_id(mac, 0x06, i);
        CHECK(sr_bloom_maybe_contains(&g_bloom, mac) == oracle_contains(&o, mac));
    }
}

static void test_zero_false_negatives(void) {
    char mac[18];
    uint32_t i;
    enum { N = 5000u };

    sr_bloom_init(&g_bloom);
    for(i = 0; i < N; i++) {
        mac_id(mac, 0x02, i);
        sr_bloom_add(&g_bloom, mac);
    }
    for(i = 0; i < N; i++) {
        uint8_t b[6];
        char lower[18];

        mac_id(mac, 0x02, i);
        CHECK(sr_bloom_maybe_contains(&g_bloom, mac) == true);

        mac_bytes(b, 0x02, i);
        mac_fmt(lower, b, kHexL);
        CHECK(sr_bloom_maybe_contains(&g_bloom, lower) == true);
    }
}

static void test_fp_table_and_fill(void) {
    static const uint32_t NS[4] = {500u, 1000u, 2000u, 4000u};
    enum { Q = 20000u };
    unsigned row;
    uint32_t fp2000 = 0;

    printf("bloom FP table (queries=%u, insert prefix=02, query prefix=06)\n", (unsigned)Q);
    printf("%6s %10s %10s %10s %10s\n", "n", "unique", "fill_bits", "fp_hits", "fp_rate");
    fflush(stdout);

    for(row = 0; row < 4u; row++) {
        uint32_t n = NS[row];
        uint32_t i;
        uint32_t hits = 0;
        uint32_t fill;
        char mac[18];

        sr_bloom_init(&g_bloom);
        for(i = 0; i < n; i++) {
            mac_id(mac, 0x02, i);
            sr_bloom_add(&g_bloom, mac);
        }
        for(i = 0; i < Q; i++) {
            mac_id(mac, 0x06, i);
            if(sr_bloom_maybe_contains(&g_bloom, mac)) {
                hits++;
            }
        }
        fill = sr_bloom_fill_bits(&g_bloom);
        printf(
            "%6u %10u %10u %10u %10.6f\n",
            (unsigned)n,
            (unsigned)sr_bloom_unique(&g_bloom),
            (unsigned)fill,
            (unsigned)hits,
            (double)hits / (double)Q);
        fflush(stdout);

        if(n == 2000u) {
            fp2000 = hits;
            /* theoretical fill ≈ 7100; ±15% → [6035, 8165] */
            CHECK(fill >= 6035u);
            CHECK(fill <= 8165u);
        }
    }

    CHECK(fp2000 * 100u < (uint32_t)Q); /* FP < 1% when n=2000 */
}

/* -------------------------------------------------------------------------- */
/* Fuzz: the seed is hardcoded. Measure coverage numbers first, then lock the */
/* floors in as CHECKs.                                                       */
/* -------------------------------------------------------------------------- */

#define SR_BLOOM_FUZZ_ITERS 20000u
#define SR_BLOOM_FUZZ_SEED 0xB1004F11u

static uint32_t xs32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void test_fuzz(void) {
    static OracleBloom o;
    enum { RECENT_CAP = 80 };
    static char recent[32][RECENT_CAP];
    uint32_t seed = SR_BLOOM_FUZZ_SEED;
    uint32_t iter;
    uint32_t rec_n = 0;
    uint32_t cov_add_new = 0, cov_add_dup = 0;
    uint32_t cov_hit = 0, cov_miss = 0, cov_fp = 0;
    uint32_t cov_case = 0, cov_null = 0, cov_empty = 0;
    uint32_t cov_nonmac = 0, cov_long = 0;
    uint32_t cov_oracle = 0;
    int reported = 0;

    sr_bloom_init(&g_bloom);
    sr_bloom_init(&g_bloom_b);
    oracle_init(&o);

    /* the NULL / empty-string paths must be hit by fuzzing */
    CHECK(sr_bloom_add(NULL, "x") == false);
    CHECK(sr_bloom_add(&g_bloom, NULL) == false);
    CHECK(sr_bloom_add(&g_bloom, "") == false);
    CHECK(sr_bloom_maybe_contains(NULL, "x") == false);
    cov_null += 4;
    cov_empty += 1;

    for(iter = 0; iter < SR_BLOOM_FUZZ_ITERS; iter++) {
        uint32_t kind = xs32(&seed) % 8u;
        char buf[RECENT_CAP];
        bool impl;
        bool ref;

        if(kind == 0) {
            /* random non-MAC byte string */
            size_t n = (size_t)(xs32(&seed) % 40u);
            size_t i;
            for(i = 0; i < n; i++) {
                buf[i] = (char)(xs32(&seed) & 0xFFu);
                if(buf[i] == '\0') {
                    buf[i] = 'x';
                }
            }
            buf[n] = '\0';
            if(n == 0) {
                cov_empty++;
            }
            if(n > 17u) {
                cov_long++;
            }
            cov_nonmac++;
        } else if(kind == 1 && rec_n > 0) {
            /* replay an already-inserted entry (including case transforms) */
            uint32_t pick = xs32(&seed) % rec_n;
            size_t i;
            memcpy(buf, recent[pick % 32u], sizeof(buf));
            if((xs32(&seed) & 1u) != 0) {
                for(i = 0; buf[i] != '\0'; i++) {
                    if(buf[i] >= 'A' && buf[i] <= 'F') {
                        buf[i] = (char)(buf[i] - 'A' + 'a');
                    } else if(buf[i] >= 'a' && buf[i] <= 'f') {
                        buf[i] = (char)(buf[i] - 'a' + 'A');
                    }
                }
                cov_case++;
            }
        } else {
            mac_id(buf, (uint8_t)(0x0Au + (xs32(&seed) % 4u)), xs32(&seed));
        }

        impl = sr_bloom_add(&g_bloom, buf);
        ref = oracle_add(&o, buf);
        if(impl != ref) {
            sr_test_failures++;
            if(reported < 5) {
                fprintf(stderr, "fuzz add mismatch: iter=%u seed=0x%08X buf=<%s>\n", iter, SR_BLOOM_FUZZ_SEED, buf);
                reported++;
            }
        } else {
            cov_oracle++;
        }
        if(impl) {
            cov_add_new++;
        } else if(buf[0] != '\0') {
            cov_add_dup++;
        }

        if(buf[0] != '\0') {
            size_t slot = rec_n % 32u;
            memset(recent[slot], 0, RECENT_CAP);
            strncpy(recent[slot], buf, RECENT_CAP - 1u);
            rec_n++;
        }

        impl = sr_bloom_maybe_contains(&g_bloom, buf);
        ref = oracle_contains(&o, buf);
        if(impl != ref) {
            sr_test_failures++;
            if(reported < 5) {
                fprintf(stderr, "fuzz contains mismatch: iter=%u\n", iter);
                reported++;
            }
        } else if(buf[0] == '\0') {
            CHECK(impl == false);
        } else if(impl != true) {
            sr_test_failures++;
            if(reported < 5) {
                fprintf(stderr, "fuzz false negative: iter=%u\n", iter);
                reported++;
            }
        } else {
            cov_hit++;
        }

        /* a query guaranteed to be outside the insert prefix */
        {
            char q[18];
            mac_id(q, 0xF0, xs32(&seed));
            impl = sr_bloom_maybe_contains(&g_bloom, q);
            ref = oracle_contains(&o, q);
            if(impl != ref) {
                sr_test_failures++;
            } else if(impl) {
                cov_fp++; /* an out-of-domain query hit = false positive */
            } else {
                cov_miss++; /* an out-of-domain query miss = true negative */
            }
        }
    }

    CHECK(sr_bloom_unique(&g_bloom) == o.unique);
    CHECK(g_bloom.added == o.added);
    CHECK(sr_bloom_fill_bits(&g_bloom) == oracle_fill_bits(&o));

    fprintf(
        stderr,
        "bloom fuzz coverage: new=%u dup=%u hit=%u miss=%u fp=%u case=%u "
        "null=%u empty=%u nonmac=%u long=%u oracle=%u unique=%u fill=%u\n",
        cov_add_new,
        cov_add_dup,
        cov_hit,
        cov_miss,
        cov_fp,
        cov_case,
        cov_null,
        cov_empty,
        cov_nonmac,
        cov_long,
        cov_oracle,
        sr_bloom_unique(&g_bloom),
        sr_bloom_fill_bits(&g_bloom));

    /*
     * Coverage floor. Seed 0xB1004F11 is hardcoded, so the numbers are deterministic.
     * Thresholds are set to roughly 1/3 of the measured values: room for minor tweaks, but it
     * goes red the moment some path stops being exercised.
     * Measured: new=13565 dup=6369 hit=19934 miss=15531 fp=4469
     * case=1245 null=4 empty=67 nonmac=2514 long=1398 oracle=20000.
     *
     * miss / fp must be counted separately: an out-of-domain query that hits is a false positive,
     * one that misses is a true negative. If both branches fed the same counter, that counter
     * would always equal the iteration count and the floor assertion would always hold -- i.e.,
     * untested (fixed during the lead session review on 2026-08-16).
     */
    CHECK(cov_add_new > 4500u);
    CHECK(cov_add_dup > 2000u);
    CHECK(cov_hit > 6000u);
    CHECK(cov_miss > 5000u);
    CHECK(cov_fp > 1400u);
    CHECK(cov_case > 400u);
    CHECK(cov_null > 0u);
    CHECK(cov_empty > 20u);
    CHECK(cov_nonmac > 800u);
    CHECK(cov_long > 400u);
    CHECK(cov_oracle > 6000u);
}

int test_bloom_run(void) {
    sr_test_failures = 0;
    test_sizeof_and_init();
    test_null_and_empty();
    test_fnv_vectors();
    test_case_fold();
    test_duplicate_and_reinit();
    test_oracle_agreement();
    test_zero_false_negatives();
    test_fp_table_and_fill();
    test_fuzz();
    return sr_test_failures;
}
