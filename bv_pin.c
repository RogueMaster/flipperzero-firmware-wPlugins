#include "bv_pin.h"
#include "bv_crypto.h"

#include <furi.h>
#include <string.h>

#define TAG "BioVaultPin"

// --- SHA-256 (FIPS 180-4), vendored: the SDK exports no hash to FAPs ---

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buf[64];
    size_t buf_len;
} Sha256;

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(Sha256* s, const uint8_t* p) {
    uint32_t w[64];
    for(int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    }
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->state[0], b = s->state[1], c = s->state[2], d = s->state[3];
    uint32_t e = s->state[4], f = s->state[5], g = s->state[6], h = s->state[7];
    for(int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->state[0] += a; s->state[1] += b; s->state[2] += c; s->state[3] += d;
    s->state[4] += e; s->state[5] += f; s->state[6] += g; s->state[7] += h;
}

static void sha256_init(Sha256* s) {
    static const uint32_t iv[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->state, iv, sizeof(iv));
    s->bitlen = 0;
    s->buf_len = 0;
}

static void sha256_update(Sha256* s, const uint8_t* data, size_t len) {
    s->bitlen += (uint64_t)len * 8;
    while(len) {
        size_t n = 64 - s->buf_len;
        if(n > len) n = len;
        memcpy(s->buf + s->buf_len, data, n);
        s->buf_len += n;
        data += n;
        len -= n;
        if(s->buf_len == 64) {
            sha256_block(s, s->buf);
            s->buf_len = 0;
        }
    }
}

static void sha256_final(Sha256* s, uint8_t out[32]) {
    uint64_t bits = s->bitlen;
    uint8_t pad = 0x80;
    sha256_update(s, &pad, 1);
    uint8_t z = 0;
    while(s->buf_len != 56) sha256_update(s, &z, 1);
    uint8_t lb[8];
    for(int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - i * 8));
    sha256_update(s, lb, 8);
    for(int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(s->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(s->state[i]);
    }
}

// --- HMAC-SHA256 + PBKDF2 (dkLen fixed at 32: a single block) ---

static void hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* msg,
    size_t msg_len,
    const uint8_t* msg2,
    size_t msg2_len,
    uint8_t out[32]) {
    uint8_t k[64] = {0};
    if(key_len > 64) {
        Sha256 s;
        sha256_init(&s);
        sha256_update(&s, key, key_len);
        sha256_final(&s, k);
    } else {
        memcpy(k, key, key_len);
    }
    uint8_t pad[64];
    Sha256 s;
    sha256_init(&s);
    for(int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    sha256_update(&s, pad, 64);
    if(msg_len) sha256_update(&s, msg, msg_len);
    if(msg2_len) sha256_update(&s, msg2, msg2_len);
    uint8_t inner[32];
    sha256_final(&s, inner);
    sha256_init(&s);
    for(int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    sha256_update(&s, pad, 64);
    sha256_update(&s, inner, 32);
    sha256_final(&s, out);
    memset(k, 0, sizeof(k));
    memset(pad, 0, sizeof(pad));
    memset(inner, 0, sizeof(inner));
}

static void pbkdf2_sha256(
    const char* pass,
    const uint8_t* salt,
    size_t salt_len,
    uint32_t iters,
    uint8_t out[32]) {
    // F(P, S, c, 1): U1 = HMAC(P, S || INT(1)); Ui = HMAC(P, Ui-1); xor all.
    uint8_t block1[BV_PIN_SALT_SIZE + 4];
    furi_check(salt_len <= BV_PIN_SALT_SIZE);
    memcpy(block1, salt, salt_len);
    block1[salt_len] = 0;
    block1[salt_len + 1] = 0;
    block1[salt_len + 2] = 0;
    block1[salt_len + 3] = 1;

    uint8_t u[32];
    size_t plen = strlen(pass);
    hmac_sha256((const uint8_t*)pass, plen, block1, salt_len + 4, NULL, 0, u);
    memcpy(out, u, 32);
    for(uint32_t i = 1; i < iters; i++) {
        hmac_sha256((const uint8_t*)pass, plen, u, 32, NULL, 0, u);
        for(int j = 0; j < 32; j++) out[j] ^= u[j];
    }
    memset(u, 0, sizeof(u));
}

// --- Public API ---

bool bv_pin_derive(
    const char* pin,
    const uint8_t salt[BV_PIN_SALT_SIZE],
    uint32_t sw_iters,
    uint32_t hw_iters,
    uint8_t out[BV_PIN_KEY_SIZE]) {
    if(!pin || !pin[0] || sw_iters == 0 || hw_iters == 0) return false;
    uint32_t t0 = furi_get_tick();
    pbkdf2_sha256(pin, salt, BV_PIN_SALT_SIZE, sw_iters, out);
    uint32_t t1 = furi_get_tick();
    bool ok = bv_crypto_kek_stretch(salt, out, hw_iters);
    FURI_LOG_I(
        TAG,
        "derive: pbkdf2(%lu)=%lums enclave(%lu)=%lums",
        (unsigned long)sw_iters,
        (unsigned long)(t1 - t0),
        (unsigned long)hw_iters,
        (unsigned long)(furi_get_tick() - t1));
    return ok;
}

// RFC 6070-style PBKDF2-HMAC-SHA256 vector (password/salt, c=1, dkLen=32).
static const uint8_t PBKDF2_KAT[32] = {
    0x12, 0x0f, 0xb6, 0xcf, 0xfc, 0xf8, 0xb3, 0x2c, 0x43, 0xe7, 0x22, 0x52,
    0x56, 0xc4, 0xf8, 0x37, 0xa8, 0x65, 0x48, 0xc9, 0x2c, 0xcc, 0x35, 0x48,
    0x08, 0x05, 0x98, 0x7c, 0xb7, 0x0b, 0xe1, 0x7b};

bool bv_pin_selftest(void) {
    uint8_t dk[32];
    pbkdf2_sha256("password", (const uint8_t*)"salt", 4, 1, dk);
    bool kat_ok = memcmp(dk, PBKDF2_KAT, 32) == 0;

    // Full derivation must be deterministic and PIN-sensitive.
    uint8_t salt[BV_PIN_SALT_SIZE];
    memset(salt, 0x5a, sizeof(salt));
    uint8_t k1[32], k2[32], k3[32];
    bool ok = bv_pin_derive("1234", salt, 8, 8, k1) && bv_pin_derive("1234", salt, 8, 8, k2) &&
              bv_pin_derive("1235", salt, 8, 8, k3);
    bool det_ok = ok && (memcmp(k1, k2, 32) == 0) && (memcmp(k1, k3, 32) != 0);

    FURI_LOG_I(TAG, "PIN KDF self-test: kat=%d det=%d", kat_ok, det_ok);
    memset(k1, 0, 32);
    memset(k2, 0, 32);
    memset(k3, 0, 32);
    return kat_ok && det_ok;
}
