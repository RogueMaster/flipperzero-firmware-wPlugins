// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT
//
// Compact SHA-1/256/512 for HMAC over short messages (TOTP only needs that).
// Message schedules are rolling 16-word buffers to keep stack use low.

#include "vf_totp.h"

#include <string.h>

/* ---------- helpers ---------- */

static void vf_totp_wipe_buf(void* p, size_t n) {
    volatile uint8_t* v = p;
    while(n--)
        *v++ = 0;
}

static uint32_t rol32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static uint32_t ror32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static uint64_t ror64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t be64(const uint8_t* p) {
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static void put32(uint8_t* p, uint32_t v) {
    for(int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (24 - 8 * i));
}

static void put64(uint8_t* p, uint64_t v) {
    put32(p, (uint32_t)(v >> 32));
    put32(p + 4, (uint32_t)v);
}

/* ---------- SHA-1 ---------- */

static void sha1_block(uint32_t h[5], const uint8_t* p) {
    uint32_t w[16];
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for(int i = 0; i < 16; i++)
        w[i] = be32(p + 4 * i);
    for(int i = 0; i < 80; i++) {
        if(i >= 16) {
            w[i & 15] = rol32(w[(i + 13) & 15] ^ w[(i + 8) & 15] ^ w[(i + 2) & 15] ^ w[i & 15], 1);
        }
        uint32_t f, k;
        if(i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if(i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if(i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t t = rol32(a, 5) + f + e + k + w[i & 15];
        e = d;
        d = c;
        c = rol32(b, 30);
        b = a;
        a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    vf_totp_wipe_buf(w, sizeof(w));
}

/* ---------- SHA-256 ---------- */

static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

static const uint32_t H256[8] = {
    0x6a09e667u,
    0xbb67ae85u,
    0x3c6ef372u,
    0xa54ff53au,
    0x510e527fu,
    0x9b05688cu,
    0x1f83d9abu,
    0x5be0cd19u,
};

static void sha256_block(uint32_t h[8], const uint8_t* p) {
    uint32_t w[16], s[8];
    memcpy(s, h, sizeof(s));
    for(int i = 0; i < 16; i++)
        w[i] = be32(p + 4 * i);
    for(int i = 0; i < 64; i++) {
        if(i >= 16) {
            uint32_t w15 = w[(i + 1) & 15], w2 = w[(i + 14) & 15];
            w[i & 15] += (ror32(w15, 7) ^ ror32(w15, 18) ^ (w15 >> 3)) + w[(i + 9) & 15] +
                         (ror32(w2, 17) ^ ror32(w2, 19) ^ (w2 >> 10));
        }
        uint32_t t1 = s[7] + (ror32(s[4], 6) ^ ror32(s[4], 11) ^ ror32(s[4], 25)) +
                      ((s[4] & s[5]) ^ (~s[4] & s[6])) + K256[i] + w[i & 15];
        uint32_t t2 = (ror32(s[0], 2) ^ ror32(s[0], 13) ^ ror32(s[0], 22)) +
                      ((s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]));
        memmove(s + 1, s, 7 * sizeof(s[0]));
        s[4] += t1;
        s[0] = t1 + t2;
    }
    for(int i = 0; i < 8; i++)
        h[i] += s[i];
    vf_totp_wipe_buf(w, sizeof(w));
    vf_totp_wipe_buf(s, sizeof(s));
}

/* ---------- SHA-512 ---------- */

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

static const uint64_t H512[8] = {
    0x6a09e667f3bcc908ULL,
    0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL,
    0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL,
    0x5be0cd19137e2179ULL,
};

static void sha512_block(uint64_t h[8], const uint8_t* p) {
    uint64_t w[16], s[8];
    memcpy(s, h, sizeof(s));
    for(int i = 0; i < 16; i++)
        w[i] = be64(p + 8 * i);
    for(int i = 0; i < 80; i++) {
        if(i >= 16) {
            uint64_t w15 = w[(i + 1) & 15], w2 = w[(i + 14) & 15];
            w[i & 15] += (ror64(w15, 1) ^ ror64(w15, 8) ^ (w15 >> 7)) + w[(i + 9) & 15] +
                         (ror64(w2, 19) ^ ror64(w2, 61) ^ (w2 >> 6));
        }
        uint64_t t1 = s[7] + (ror64(s[4], 14) ^ ror64(s[4], 18) ^ ror64(s[4], 41)) +
                      ((s[4] & s[5]) ^ (~s[4] & s[6])) + K512[i] + w[i & 15];
        uint64_t t2 = (ror64(s[0], 28) ^ ror64(s[0], 34) ^ ror64(s[0], 39)) +
                      ((s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]));
        memmove(s + 1, s, 7 * sizeof(s[0]));
        s[4] += t1;
        s[0] = t1 + t2;
    }
    for(int i = 0; i < 8; i++)
        h[i] += s[i];
    vf_totp_wipe_buf(w, sizeof(w));
    vf_totp_wipe_buf(s, sizeof(s));
}

/* ---------- hash and HMAC over short messages ---------- */

#define VF_HASH_MAX_IN 192u /* block (128) + digest (64) */

static size_t vf_digest_size(VfTotpAlgo algo) {
    return algo == VfTotpSha512 ? 64 : algo == VfTotpSha256 ? 32 : 20;
}

static size_t vf_block_size(VfTotpAlgo algo) {
    return algo == VfTotpSha512 ? 128 : 64;
}

/* One-shot hash of len <= VF_HASH_MAX_IN bytes. */
static void vf_hash(VfTotpAlgo algo, const uint8_t* msg, size_t len, uint8_t* out) {
    uint8_t buf[256];
    size_t bs = vf_block_size(algo);
    size_t len_field = bs == 128 ? 16 : 8;
    size_t total = (len + 1 + len_field + bs - 1) / bs * bs;
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    memset(buf + len + 1, 0, total - len - 1);
    uint64_t bits = (uint64_t)len << 3;
    for(int i = 0; i < 8; i++)
        buf[total - 1 - i] = (uint8_t)(bits >> (8 * i));

    if(algo == VfTotpSha512) {
        uint64_t h[8];
        memcpy(h, H512, sizeof(h));
        for(size_t o = 0; o < total; o += bs)
            sha512_block(h, buf + o);
        for(int i = 0; i < 8; i++)
            put64(out + 8 * i, h[i]);
        vf_totp_wipe_buf(h, sizeof(h));
    } else if(algo == VfTotpSha256) {
        uint32_t h[8];
        memcpy(h, H256, sizeof(h));
        for(size_t o = 0; o < total; o += bs)
            sha256_block(h, buf + o);
        for(int i = 0; i < 8; i++)
            put32(out + 4 * i, h[i]);
        vf_totp_wipe_buf(h, sizeof(h));
    } else {
        uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
        for(size_t o = 0; o < total; o += bs)
            sha1_block(h, buf + o);
        for(int i = 0; i < 5; i++)
            put32(out + 4 * i, h[i]);
        vf_totp_wipe_buf(h, sizeof(h));
    }
    vf_totp_wipe_buf(buf, sizeof(buf));
}

/* HMAC with key_len <= 64 and msg_len <= 64. */
static void vf_hmac(
    VfTotpAlgo algo,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* msg,
    size_t msg_len,
    uint8_t* mac) {
    uint8_t buf[VF_HASH_MAX_IN];
    uint8_t k[128] = {0};
    size_t bs = vf_block_size(algo);
    size_t ds = vf_digest_size(algo);
    if(key_len > bs)
        vf_hash(algo, key, key_len, k);
    else
        memcpy(k, key, key_len);
    for(size_t i = 0; i < bs; i++)
        buf[i] = k[i] ^ 0x36;
    memcpy(buf + bs, msg, msg_len);
    vf_hash(algo, buf, bs + msg_len, mac);
    for(size_t i = 0; i < bs; i++)
        buf[i] = k[i] ^ 0x5C;
    memcpy(buf + bs, mac, ds);
    vf_hash(algo, buf, bs + ds, mac);
    vf_totp_wipe_buf(buf, sizeof(buf));
    vf_totp_wipe_buf(k, sizeof(k));
}

/* ---------- TOTP ---------- */

static int vf_b32(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a';
    if(c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

/* Reads digits up to the next ':'; returns the position after it or NULL. */
static const char* vf_read_num(const char* p, uint32_t* out) {
    uint32_t v = 0;
    const char* start = p;
    while(*p >= '0' && *p <= '9' && p - start < 5)
        v = v * 10 + (uint32_t)(*p++ - '0');
    if(p == start || *p != ':') return NULL;
    *out = v;
    return p + 1;
}

bool vf_totp_parse(const char* field, VfTotp* t) {
    memset(t, 0, sizeof(*t));
    t->algo = VfTotpSha1;
    t->digits = 6;
    t->period = 30;
    if(!field || !field[0]) return false;

    const char* secret = field;
    const char* colon = strchr(field, ':');
    if(colon) {
        size_t n = (size_t)(colon - field);
        if(n == 4 && memcmp(field, "SHA1", 4) == 0)
            t->algo = VfTotpSha1;
        else if(n == 6 && memcmp(field, "SHA256", 6) == 0)
            t->algo = VfTotpSha256;
        else if(n == 6 && memcmp(field, "SHA512", 6) == 0)
            t->algo = VfTotpSha512;
        else if(n == 5 && memcmp(field, "STEAM", 5) == 0)
            t->algo = VfTotpSteam;
        else
            return false;
        uint32_t digits, period;
        const char* p = vf_read_num(colon + 1, &digits);
        if(!p) return false;
        p = vf_read_num(p, &period);
        if(!p) return false;
        if(digits < 1 || digits > 10 || period < 1 || period > 3600) return false;
        t->digits = (uint8_t)digits;
        t->period = (uint16_t)period;
        secret = p;
    }

    uint32_t acc = 0;
    int bits = 0;
    size_t n = 0;
    for(const char* p = secret; *p; p++) {
        if(*p == '=' || *p == ' ') continue;
        int v = vf_b32(*p);
        if(v < 0) return false;
        acc = (acc << 5) | (uint32_t)v;
        bits += 5;
        if(bits >= 8) {
            if(n >= VF_TOTP_MAX_KEY) return false;
            bits -= 8;
            t->key[n++] = (uint8_t)(acc >> bits);
        }
    }
    t->key_len = (uint8_t)n;
    return n > 0;
}

void vf_totp_code(const VfTotp* t, uint64_t unix_time, char out[VF_TOTP_CODE_MAX]) {
    uint64_t counter = unix_time / (t->period ? t->period : 30);
    uint8_t msg[8];
    put64(msg, counter);
    VfTotpAlgo hash = t->algo == VfTotpSteam ? VfTotpSha1 : t->algo;
    uint8_t mac[64];
    vf_hmac(hash, t->key, t->key_len, msg, sizeof(msg), mac);
    size_t ds = vf_digest_size(hash);
    uint8_t off = mac[ds - 1] & 0x0F;
    uint32_t bin = ((uint32_t)(mac[off] & 0x7F) << 24) | ((uint32_t)mac[off + 1] << 16) |
                   ((uint32_t)mac[off + 2] << 8) | mac[off + 3];
    uint8_t digits = t->digits > 10 ? 10 : t->digits;
    if(t->algo == VfTotpSteam) {
        static const char alphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
        for(uint8_t i = 0; i < digits; i++) {
            out[i] = alphabet[bin % 26u];
            bin /= 26u;
        }
    } else {
        for(int i = digits - 1; i >= 0; i--) {
            out[i] = (char)('0' + bin % 10u);
            bin /= 10u;
        }
    }
    out[digits] = 0;
    vf_totp_wipe_buf(mac, sizeof(mac));
}

uint32_t vf_totp_remaining(const VfTotp* t, uint64_t unix_time) {
    uint32_t period = t->period ? t->period : 30;
    return period - (uint32_t)(unix_time % period);
}

void vf_totp_wipe(VfTotp* t) {
    vf_totp_wipe_buf(t, sizeof(*t));
}
