#include "ulcfkey_brute.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC optimize("O3")

/*
 * Fast portable DES core and incremental key-schedule technique adapted from
 * the Proxmark3 client (cmdhfmfu.c). Keeping the changing 28-bit half-key as
 * XOR contributions avoids constructing a complete 2-key 3DES schedule for
 * every candidate.
 */

static const uint8_t phase_segment[ULCFKEY_BRUTE_TARGET_COUNT] = {2, 3, 0, 1};
static const uint8_t phase_target[ULCFKEY_BRUTE_TARGET_COUNT] = {3, 2, 1, 0};

struct UlcfkeyBruteRuntime {
    bool valid;
    uint8_t phase;
    uint32_t scheduled_candidate;
    bool candidate_in_k1;
    uint64_t ciphertext_ip_block;
    uint64_t fixed_sk[16];
    uint64_t cand_sk_base[16];
    uint64_t cand_sk_contrib[28 * 16];
    uint64_t candidate_sk[16];
};

static uint64_t mfulc_desbrute_be64(const uint8_t *b) {
    return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8) | (uint64_t)b[7];
}

static const uint8_t MFULC_DES_PC1[56] = {
    57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4
};

static const uint8_t MFULC_DES_PC2[48] = {
    14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10,
    23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32
};

static const uint8_t MFULC_DES_SHIFTS[16] = {
    1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static const uint8_t MFULC_DES_IP[64] = {
    58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7
};

static const uint8_t MFULC_DES_FP[64] = {
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25
};

static const uint32_t MFULC_DES_SP[512] = {
    0x00808200, 0x00000000, 0x00008000, 0x00808202, 0x00808002, 0x00008202, 0x00000002, 0x00008000, 0x00000200, 0x00808200, 0x00808202, 0x00000200, 0x00800202, 0x00808002, 0x00800000, 0x00000002, 0x00000202, 0x00800200, 0x00800200, 0x00008200, 0x00008200, 0x00808000, 0x00808000, 0x00800202, 0x00008002, 0x00800002, 0x00800002, 0x00008002, 0x00000000, 0x00000202, 0x00008202, 0x00800000, 0x00008000, 0x00808202, 0x00000002, 0x00808000, 0x00808200, 0x00800000, 0x00800000, 0x00000200, 0x00808002, 0x00008000, 0x00008200, 0x00800002, 0x00000200, 0x00000002, 0x00800202, 0x00008202, 0x00808202, 0x00008002, 0x00808000, 0x00800202, 0x00800002, 0x00000202, 0x00008202, 0x00808200, 0x00000202, 0x00800200, 0x00800200, 0x00000000, 0x00008002, 0x00008200, 0x00000000, 0x00808002,
    0x40084010, 0x40004000, 0x00004000, 0x00084010, 0x00080000, 0x00000010, 0x40080010, 0x40004010, 0x40000010, 0x40084010, 0x40084000, 0x40000000, 0x40004000, 0x00080000, 0x00000010, 0x40080010, 0x00084000, 0x00080010, 0x40004010, 0x00000000, 0x40000000, 0x00004000, 0x00084010, 0x40080000, 0x00080010, 0x40000010, 0x00000000, 0x00084000, 0x00004010, 0x40084000, 0x40080000, 0x00004010, 0x00000000, 0x00084010, 0x40080010, 0x00080000, 0x40004010, 0x40080000, 0x40084000, 0x00004000, 0x40080000, 0x40004000, 0x00000010, 0x40084010, 0x00084010, 0x00000010, 0x00004000, 0x40000000, 0x00004010, 0x40084000, 0x00080000, 0x40000010, 0x00080010, 0x40004010, 0x40000010, 0x00080010, 0x00084000, 0x00000000, 0x40004000, 0x00004010, 0x40000000, 0x40080010, 0x40084010, 0x00084000,
    0x00000104, 0x04010100, 0x00000000, 0x04010004, 0x04000100, 0x00000000, 0x00010104, 0x04000100, 0x00010004, 0x04000004, 0x04000004, 0x00010000, 0x04010104, 0x00010004, 0x04010000, 0x00000104, 0x04000000, 0x00000004, 0x04010100, 0x00000100, 0x00010100, 0x04010000, 0x04010004, 0x00010104, 0x04000104, 0x00010100, 0x00010000, 0x04000104, 0x00000004, 0x04010104, 0x00000100, 0x04000000, 0x04010100, 0x04000000, 0x00010004, 0x00000104, 0x00010000, 0x04010100, 0x04000100, 0x00000000, 0x00000100, 0x00010004, 0x04010104, 0x04000100, 0x04000004, 0x00000100, 0x00000000, 0x04010004, 0x04000104, 0x00010000, 0x04000000, 0x04010104, 0x00000004, 0x00010104, 0x00010100, 0x04000004, 0x04010000, 0x04000104, 0x00000104, 0x04010000, 0x00010104, 0x00000004, 0x04010004, 0x00010100,
    0x80401000, 0x80001040, 0x80001040, 0x00000040, 0x00401040, 0x80400040, 0x80400000, 0x80001000, 0x00000000, 0x00401000, 0x00401000, 0x80401040, 0x80000040, 0x00000000, 0x00400040, 0x80400000, 0x80000000, 0x00001000, 0x00400000, 0x80401000, 0x00000040, 0x00400000, 0x80001000, 0x00001040, 0x80400040, 0x80000000, 0x00001040, 0x00400040, 0x00001000, 0x00401040, 0x80401040, 0x80000040, 0x00400040, 0x80400000, 0x00401000, 0x80401040, 0x80000040, 0x00000000, 0x00000000, 0x00401000, 0x00001040, 0x00400040, 0x80400040, 0x80000000, 0x80401000, 0x80001040, 0x80001040, 0x00000040, 0x80401040, 0x80000040, 0x80000000, 0x00001000, 0x80400000, 0x80001000, 0x00401040, 0x80400040, 0x80001000, 0x00001040, 0x00400000, 0x80401000, 0x00000040, 0x00400000, 0x00001000, 0x00401040,
    0x00000080, 0x01040080, 0x01040000, 0x21000080, 0x00040000, 0x00000080, 0x20000000, 0x01040000, 0x20040080, 0x00040000, 0x01000080, 0x20040080, 0x21000080, 0x21040000, 0x00040080, 0x20000000, 0x01000000, 0x20040000, 0x20040000, 0x00000000, 0x20000080, 0x21040080, 0x21040080, 0x01000080, 0x21040000, 0x20000080, 0x00000000, 0x21000000, 0x01040080, 0x01000000, 0x21000000, 0x00040080, 0x00040000, 0x21000080, 0x00000080, 0x01000000, 0x20000000, 0x01040000, 0x21000080, 0x20040080, 0x01000080, 0x20000000, 0x21040000, 0x01040080, 0x20040080, 0x00000080, 0x01000000, 0x21040000, 0x21040080, 0x00040080, 0x21000000, 0x21040080, 0x01040000, 0x00000000, 0x20040000, 0x21000000, 0x00040080, 0x01000080, 0x20000080, 0x00040000, 0x00000000, 0x20040000, 0x01040080, 0x20000080,
    0x10000008, 0x10200000, 0x00002000, 0x10202008, 0x10200000, 0x00000008, 0x10202008, 0x00200000, 0x10002000, 0x00202008, 0x00200000, 0x10000008, 0x00200008, 0x10002000, 0x10000000, 0x00002008, 0x00000000, 0x00200008, 0x10002008, 0x00002000, 0x00202000, 0x10002008, 0x00000008, 0x10200008, 0x10200008, 0x00000000, 0x00202008, 0x10202000, 0x00002008, 0x00202000, 0x10202000, 0x10000000, 0x10002000, 0x00000008, 0x10200008, 0x00202000, 0x10202008, 0x00200000, 0x00002008, 0x10000008, 0x00200000, 0x10002000, 0x10000000, 0x00002008, 0x10000008, 0x10202008, 0x00202000, 0x10200000, 0x00202008, 0x10202000, 0x00000000, 0x10200008, 0x00000008, 0x00002000, 0x10200000, 0x00202008, 0x00002000, 0x00200008, 0x10002008, 0x00000000, 0x10202000, 0x10000000, 0x00200008, 0x10002008,
    0x00100000, 0x02100001, 0x02000401, 0x00000000, 0x00000400, 0x02000401, 0x00100401, 0x02100400, 0x02100401, 0x00100000, 0x00000000, 0x02000001, 0x00000001, 0x02000000, 0x02100001, 0x00000401, 0x02000400, 0x00100401, 0x00100001, 0x02000400, 0x02000001, 0x02100000, 0x02100400, 0x00100001, 0x02100000, 0x00000400, 0x00000401, 0x02100401, 0x00100400, 0x00000001, 0x02000000, 0x00100400, 0x02000000, 0x00100400, 0x00100000, 0x02000401, 0x02000401, 0x02100001, 0x02100001, 0x00000001, 0x00100001, 0x02000000, 0x02000400, 0x00100000, 0x02100400, 0x00000401, 0x00100401, 0x02100400, 0x00000401, 0x02000001, 0x02100401, 0x02100000, 0x00100400, 0x00000000, 0x00000001, 0x02100401, 0x00000000, 0x00100401, 0x02100000, 0x00000400, 0x02000001, 0x02000400, 0x00000400, 0x00100001,
    0x08000820, 0x00000800, 0x00020000, 0x08020820, 0x08000000, 0x08000820, 0x00000020, 0x08000000, 0x00020020, 0x08020000, 0x08020820, 0x00020800, 0x08020800, 0x00020820, 0x00000800, 0x00000020, 0x08020000, 0x08000020, 0x08000800, 0x00000820, 0x00020800, 0x00020020, 0x08020020, 0x08020800, 0x00000820, 0x00000000, 0x00000000, 0x08020020, 0x08000020, 0x08000800, 0x00020820, 0x00020000, 0x00020820, 0x00020000, 0x08020800, 0x00000800, 0x00000020, 0x08020020, 0x00000800, 0x00020820, 0x08000800, 0x00000020, 0x08000020, 0x08020000, 0x08020020, 0x08000000, 0x00020000, 0x08000820, 0x00000000, 0x08020820, 0x00020020, 0x08000020, 0x08020000, 0x08000800, 0x08000820, 0x00000000, 0x08020820, 0x00020800, 0x00020800, 0x00000820, 0x00000820, 0x00020020, 0x08000000, 0x08020800
};

static uint64_t mfulc_desbrute_perm(uint64_t src, int src_bits, const uint8_t *tbl, int n) {
    uint64_t dst = 0;
    for (int i = 0; i < n; i++) {
        int sb = tbl[i] - 1;
        dst |= ((src >> (src_bits - 1 - sb)) & 1ULL) << (n - 1 - i);
    }
    return dst;
}

static void mfulc_desbrute_keyschedule(uint64_t key64, uint64_t sk[16]) {
    uint64_t key56 = mfulc_desbrute_perm(key64, 64, MFULC_DES_PC1, 56);
    uint32_t c = (uint32_t)(key56 >> 28) & 0x0FFFFFFF;
    uint32_t d = (uint32_t)key56 & 0x0FFFFFFF;

    for (int i = 0; i < 16; i++) {
        int s = MFULC_DES_SHIFTS[i];
        c = ((c << s) | (c >> (28 - s))) & 0x0FFFFFFF;
        d = ((d << s) | (d >> (28 - s))) & 0x0FFFFFFF;
        sk[i] = mfulc_desbrute_perm(((uint64_t)c << 28) | d, 56, MFULC_DES_PC2, 48);
    }
}

static uint32_t mfulc_desbrute_f(uint32_t r, uint64_t k) {
    uint32_t r0 = ((r & 1u) << 5) | ((r >> 27) & 0x1Fu);
    uint32_t r1 = (r >> 23) & 0x3Fu;
    uint32_t r2 = (r >> 19) & 0x3Fu;
    uint32_t r3 = (r >> 15) & 0x3Fu;
    uint32_t r4 = (r >> 11) & 0x3Fu;
    uint32_t r5 = (r >> 7) & 0x3Fu;
    uint32_t r6 = (r >> 3) & 0x3Fu;
    uint32_t r7 = ((r & 0x1Fu) << 1) | ((r >> 31) & 1u);

    r0 ^= (uint32_t)((k >> 42) & 0x3Fu);
    r1 ^= (uint32_t)((k >> 36) & 0x3Fu);
    r2 ^= (uint32_t)((k >> 30) & 0x3Fu);
    r3 ^= (uint32_t)((k >> 24) & 0x3Fu);
    r4 ^= (uint32_t)((k >> 18) & 0x3Fu);
    r5 ^= (uint32_t)((k >> 12) & 0x3Fu);
    r6 ^= (uint32_t)((k >> 6) & 0x3Fu);
    r7 ^= (uint32_t)(k & 0x3Fu);

    return MFULC_DES_SP[r0] ^ MFULC_DES_SP[64 + r1] ^ MFULC_DES_SP[128 + r2] ^ MFULC_DES_SP[192 + r3] ^
           MFULC_DES_SP[256 + r4] ^ MFULC_DES_SP[320 + r5] ^ MFULC_DES_SP[384 + r6] ^ MFULC_DES_SP[448 + r7];
}

static uint64_t mfulc_desbrute_des_rounds(uint64_t ip_block, const uint64_t sk[16], bool decrypt) {
    uint32_t l = (uint32_t)(ip_block >> 32);
    uint32_t r = (uint32_t)ip_block;

    for (int i = 0; i < 16; i++) {
        uint64_t k = decrypt ? sk[15 - i] : sk[i];
        uint32_t nr = l ^ mfulc_desbrute_f(r, k);
        l = r;
        r = nr;
    }
    return ((uint64_t)r << 32) | l;
}

static uint64_t mfulc_desbrute_tdea2_dec_ip(uint64_t ip_block, const uint64_t k1_sk[16], const uint64_t k2_sk[16]) {
    uint64_t t = mfulc_desbrute_des_rounds(ip_block, k1_sk, true);
    t = mfulc_desbrute_des_rounds(t, k2_sk, false);
    t = mfulc_desbrute_des_rounds(t, k1_sk, true);
    return mfulc_desbrute_perm(t, 64, MFULC_DES_FP, 64);
}
static void mfulc_desbrute_compute_sk_tables(const uint8_t base_half[8], int var_offset, uint64_t sk_base[16], uint64_t sk_contrib[28 * 16]) {
    uint8_t half[8] = {0};

    memcpy(half, base_half, sizeof(half));
    half[var_offset] = 0;
    half[var_offset + 1] = 0;
    half[var_offset + 2] = 0;
    half[var_offset + 3] = 0;
    mfulc_desbrute_keyschedule(mfulc_desbrute_be64(half), sk_base);

    for (int bit = 0; bit < 28; bit++) {
        uint8_t tmp[8] = {0};
        int byte_in_half = bit / 7;
        int bit_in_byte = bit % 7;
        uint64_t bit_sk[16] = {0};

        tmp[var_offset + byte_in_half] = (uint8_t)(1u << (bit_in_byte + 1));
        mfulc_desbrute_keyschedule(mfulc_desbrute_be64(tmp), bit_sk);

        for (int round = 0; round < 16; round++) {
            sk_contrib[(bit * 16) + round] = bit_sk[round];
        }
    }
}

static void mfulc_desbrute_make_candidate_sk(const UlcfkeyBruteRuntime *args, uint32_t idx, uint64_t cand_sk[16]) {
    memcpy(cand_sk, args->cand_sk_base, sizeof(args->cand_sk_base));

    for (int bit = 0; bit < 28; bit++) {
        if ((idx >> bit) & 1u) {
            const uint64_t *contrib = &args->cand_sk_contrib[bit * 16];
            for (int round = 0; round < 16; round++) {
                cand_sk[round] ^= contrib[round];
            }
        }
    }
}

static void mfulc_desbrute_update_candidate_sk(const UlcfkeyBruteRuntime *args, uint32_t old_idx, uint32_t new_idx, uint64_t cand_sk[16]) {
    uint32_t changed = old_idx ^ new_idx;

    while (changed != 0) {
        int bit = __builtin_ctz(changed);
        const uint64_t *contrib = &args->cand_sk_contrib[bit * 16];

        for (int round = 0; round < 16; round++) {
            cand_sk[round] ^= contrib[round];
        }
        changed &= changed - 1;
    }
}

static bool ulcfkey_valid_ulcg_lfsr_be(uint64_t plaintext) {
    uint16_t value = (uint16_t)(plaintext >> 48);
    value = (uint16_t)((value << 15) |
                       ((value >> 1) ^ (((value >> 3) ^ (value >> 4) ^ (value >> 6)) & 1U)));
    if(value != (uint16_t)(plaintext >> 32)) return false;
    value = (uint16_t)((value << 15) |
                       ((value >> 1) ^ (((value >> 3) ^ (value >> 4) ^ (value >> 6)) & 1U)));
    if(value != (uint16_t)(plaintext >> 16)) return false;
    value = (uint16_t)((value << 15) |
                       ((value >> 1) ^ (((value >> 3) ^ (value >> 4) ^ (value >> 6)) & 1U)));
    return value == (uint16_t)plaintext;
}

static void ulcfkey_candidate_bytes(uint32_t candidate, uint8_t out[4]) {
    out[0] = (uint8_t)((candidate & 0x7FU) << 1);
    out[1] = (uint8_t)(((candidate >> 7) & 0x7FU) << 1);
    out[2] = (uint8_t)(((candidate >> 14) & 0x7FU) << 1);
    out[3] = (uint8_t)(((candidate >> 21) & 0x7FU) << 1);
}

static void ulcfkey_runtime_prepare(
    UlcfkeyBruteRuntime* runtime,
    const UlcfkeyBruteState* state,
    const uint8_t target[8]) {
    const uint8_t segment = phase_segment[state->phase];
    runtime->phase = state->phase;
    runtime->candidate_in_k1 = segment < 2U;
    const uint8_t var_offset = (uint8_t)((segment & 1U) * 4U);
    const uint8_t* candidate_half = state->key + (runtime->candidate_in_k1 ? 0U : 8U);
    const uint8_t* fixed_half = state->key + (runtime->candidate_in_k1 ? 8U : 0U);

    runtime->ciphertext_ip_block =
        mfulc_desbrute_perm(mfulc_desbrute_be64(target), 64, MFULC_DES_IP, 64);
    mfulc_desbrute_keyschedule(mfulc_desbrute_be64(fixed_half), runtime->fixed_sk);
    mfulc_desbrute_compute_sk_tables(
        candidate_half, var_offset, runtime->cand_sk_base, runtime->cand_sk_contrib);
    mfulc_desbrute_make_candidate_sk(runtime, state->next_candidate, runtime->candidate_sk);
    runtime->scheduled_candidate = state->next_candidate;
    runtime->valid = true;
}

void ulcfkey_brute_init(UlcfkeyBruteState* state) {
    furi_check(state);
    memset(state, 0, sizeof(*state));
}

UlcfkeyBruteRuntime* ulcfkey_brute_runtime_alloc(void) {
    return calloc(1, sizeof(UlcfkeyBruteRuntime));
}

void ulcfkey_brute_runtime_free(UlcfkeyBruteRuntime* runtime) {
    if(runtime) {
        memset(runtime, 0, sizeof(*runtime));
        free(runtime);
    }
}

void ulcfkey_brute_runtime_reset(UlcfkeyBruteRuntime* runtime) {
    if(runtime) runtime->valid = false;
}

bool ulcfkey_brute_step(
    UlcfkeyBruteState* state,
    UlcfkeyBruteRuntime* runtime,
    const uint8_t targets[ULCFKEY_BRUTE_TARGET_COUNT][8],
    uint32_t budget) {
    furi_check(state);
    furi_check(runtime);
    furi_check(targets);
    if(state->complete || state->phase >= ULCFKEY_BRUTE_TARGET_COUNT) return false;

    const uint8_t segment = phase_segment[state->phase];
    const uint8_t target = phase_target[state->phase];
    if(!runtime->valid || runtime->phase != state->phase ||
       runtime->scheduled_candidate != state->next_candidate) {
        ulcfkey_runtime_prepare(runtime, state, targets[target]);
    }

    for(uint32_t count = 0;
        count < budget && state->next_candidate < ULCFKEY_BRUTE_SPACE;
        count++) {
        const uint64_t* k1 =
            runtime->candidate_in_k1 ? runtime->candidate_sk : runtime->fixed_sk;
        const uint64_t* k2 =
            runtime->candidate_in_k1 ? runtime->fixed_sk : runtime->candidate_sk;
        uint64_t plaintext = mfulc_desbrute_tdea2_dec_ip(
            runtime->ciphertext_ip_block, k1, k2);

        if(ulcfkey_valid_ulcg_lfsr_be(plaintext)) {
            uint8_t candidate_bytes[4];
            ulcfkey_candidate_bytes(state->next_candidate, candidate_bytes);
            memcpy(state->key + ((size_t)segment * 4U), candidate_bytes, 4);
            state->phase++;
            state->next_candidate = 0;
            state->complete = state->phase == ULCFKEY_BRUTE_TARGET_COUNT;
            runtime->valid = false;
            return true;
        }

        const uint32_t old_candidate = state->next_candidate;
        state->next_candidate++;
        if(state->next_candidate < ULCFKEY_BRUTE_SPACE) {
            mfulc_desbrute_update_candidate_sk(
                runtime, old_candidate, state->next_candidate, runtime->candidate_sk);
            runtime->scheduled_candidate = state->next_candidate;
        }
    }

    return false;
}

uint8_t ulcfkey_brute_segment(const UlcfkeyBruteState* state) {
    furi_check(state);
    return state->phase < ULCFKEY_BRUTE_TARGET_COUNT ? phase_segment[state->phase] : 4U;
}

uint8_t ulcfkey_brute_percent(const UlcfkeyBruteState* state) {
    furi_check(state);
    if(state->complete) return 100U;
    return (uint8_t)(((uint64_t)state->next_candidate * 100U) / ULCFKEY_BRUTE_SPACE);
}
