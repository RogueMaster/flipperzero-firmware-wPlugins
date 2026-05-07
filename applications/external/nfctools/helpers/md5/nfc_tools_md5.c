// Standalone MD5 implementation — public domain.
// Compliant with RFC 1321. Only nfc_tools_md5() is exposed.

#include "nfc_tools_md5.h"
#include <string.h>
#include <stdint.h>

// ── Internal macros ──────────────────────────────────────────────────────────

#define F(x, y, z)  ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)  ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)  ((x) ^ (y) ^ (z))
#define I_(x, y, z) ((y) ^ ((x) | (~(z))))

#define ROL32(x, s) (((x) << (s)) | ((x) >> (32u - (s))))

// Each step: a = b + ROL32(a + f(b,c,d) + w + t, s)
#define STEP(f, a, b, c, d, w, t, s) \
    (a) = (b) + ROL32((a) + f((b), (c), (d)) + (w) + (uint32_t)(t), (s))

// ── Process one or more 64-byte blocks ──────────────────────────────────────

static const uint8_t*
    md5_transform(uint32_t s[4], const uint8_t* p, size_t n) {
    while(n) {
        uint32_t a = s[0], b = s[1], c = s[2], d = s[3];

        // Load 16 32-bit words in little-endian order
        uint32_t w[16];
        for(int i = 0; i < 16; i++) {
            w[i] = (uint32_t)p[i * 4]
                 | ((uint32_t)p[i * 4 + 1] <<  8)
                 | ((uint32_t)p[i * 4 + 2] << 16)
                 | ((uint32_t)p[i * 4 + 3] << 24);
        }

        // Round 1 — F
        STEP(F, a, b, c, d, w[ 0], 0xd76aa478ul,  7);
        STEP(F, d, a, b, c, w[ 1], 0xe8c7b756ul, 12);
        STEP(F, c, d, a, b, w[ 2], 0x242070dbul, 17);
        STEP(F, b, c, d, a, w[ 3], 0xc1bdceeeul, 22);
        STEP(F, a, b, c, d, w[ 4], 0xf57c0faful,  7);
        STEP(F, d, a, b, c, w[ 5], 0x4787c62aul, 12);
        STEP(F, c, d, a, b, w[ 6], 0xa8304613ul, 17);
        STEP(F, b, c, d, a, w[ 7], 0xfd469501ul, 22);
        STEP(F, a, b, c, d, w[ 8], 0x698098d8ul,  7);
        STEP(F, d, a, b, c, w[ 9], 0x8b44f7aful, 12);
        STEP(F, c, d, a, b, w[10], 0xffff5bb1ul, 17);
        STEP(F, b, c, d, a, w[11], 0x895cd7beul, 22);
        STEP(F, a, b, c, d, w[12], 0x6b901122ul,  7);
        STEP(F, d, a, b, c, w[13], 0xfd987193ul, 12);
        STEP(F, c, d, a, b, w[14], 0xa679438eul, 17);
        STEP(F, b, c, d, a, w[15], 0x49b40821ul, 22);

        // Round 2 — G
        STEP(G, a, b, c, d, w[ 1], 0xf61e2562ul,  5);
        STEP(G, d, a, b, c, w[ 6], 0xc040b340ul,  9);
        STEP(G, c, d, a, b, w[11], 0x265e5a51ul, 14);
        STEP(G, b, c, d, a, w[ 0], 0xe9b6c7aaul, 20);
        STEP(G, a, b, c, d, w[ 5], 0xd62f105dul,  5);
        STEP(G, d, a, b, c, w[10], 0x02441453ul,  9);
        STEP(G, c, d, a, b, w[15], 0xd8a1e681ul, 14);
        STEP(G, b, c, d, a, w[ 4], 0xe7d3fbc8ul, 20);
        STEP(G, a, b, c, d, w[ 9], 0x21e1cde6ul,  5);
        STEP(G, d, a, b, c, w[14], 0xc33707d6ul,  9);
        STEP(G, c, d, a, b, w[ 3], 0xf4d50d87ul, 14);
        STEP(G, b, c, d, a, w[ 8], 0x455a14edul, 20);
        STEP(G, a, b, c, d, w[13], 0xa9e3e905ul,  5);
        STEP(G, d, a, b, c, w[ 2], 0xfcefa3f8ul,  9);
        STEP(G, c, d, a, b, w[ 7], 0x676f02d9ul, 14);
        STEP(G, b, c, d, a, w[12], 0x8d2a4c8aul, 20);

        // Round 3 — H
        STEP(H, a, b, c, d, w[ 5], 0xfffa3942ul,  4);
        STEP(H, d, a, b, c, w[ 8], 0x8771f681ul, 11);
        STEP(H, c, d, a, b, w[11], 0x6d9d6122ul, 16);
        STEP(H, b, c, d, a, w[14], 0xfde5380cul, 23);
        STEP(H, a, b, c, d, w[ 1], 0xa4beea44ul,  4);
        STEP(H, d, a, b, c, w[ 4], 0x4bdecfa9ul, 11);
        STEP(H, c, d, a, b, w[ 7], 0xf6bb4b60ul, 16);
        STEP(H, b, c, d, a, w[10], 0xbebfbc70ul, 23);
        STEP(H, a, b, c, d, w[13], 0x289b7ec6ul,  4);
        STEP(H, d, a, b, c, w[ 0], 0xeaa127faul, 11);
        STEP(H, c, d, a, b, w[ 3], 0xd4ef3085ul, 16);
        STEP(H, b, c, d, a, w[ 6], 0x04881d05ul, 23);
        STEP(H, a, b, c, d, w[ 9], 0xd9d4d039ul,  4);
        STEP(H, d, a, b, c, w[12], 0xe6db99e5ul, 11);
        STEP(H, c, d, a, b, w[15], 0x1fa27cf8ul, 16);
        STEP(H, b, c, d, a, w[ 2], 0xc4ac5665ul, 23);

        // Round 4 — I
        STEP(I_, a, b, c, d, w[ 0], 0xf4292244ul,  6);
        STEP(I_, d, a, b, c, w[ 7], 0x432aff97ul, 10);
        STEP(I_, c, d, a, b, w[14], 0xab9423a7ul, 15);
        STEP(I_, b, c, d, a, w[ 5], 0xfc93a039ul, 21);
        STEP(I_, a, b, c, d, w[12], 0x655b59c3ul,  6);
        STEP(I_, d, a, b, c, w[ 3], 0x8f0ccc92ul, 10);
        STEP(I_, c, d, a, b, w[10], 0xffeff47dul, 15);
        STEP(I_, b, c, d, a, w[ 1], 0x85845dd1ul, 21);
        STEP(I_, a, b, c, d, w[ 8], 0x6fa87e4ful,  6);
        STEP(I_, d, a, b, c, w[15], 0xfe2ce6e0ul, 10);
        STEP(I_, c, d, a, b, w[ 6], 0xa3014314ul, 15);
        STEP(I_, b, c, d, a, w[13], 0x4e0811a1ul, 21);
        STEP(I_, a, b, c, d, w[ 4], 0xf7537e82ul,  6);
        STEP(I_, d, a, b, c, w[11], 0xbd3af235ul, 10);
        STEP(I_, c, d, a, b, w[ 2], 0x2ad7d2bbul, 15);
        STEP(I_, b, c, d, a, w[ 9], 0xeb86d391ul, 21);

        s[0] += a;
        s[1] += b;
        s[2] += c;
        s[3] += d;

        p += 64;
        n -= 64;
    }
    return p;
}

// ── Public API ───────────────────────────────────────────────────────────────

void nfc_tools_md5(const uint8_t* data, size_t len, uint8_t digest[16]) {
    uint32_t state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};

    // Total length in bits (little-endian 64-bit)
    uint32_t bits_lo = (uint32_t)(len << 3);
    uint32_t bits_hi = (uint32_t)((uint64_t)len >> 29);

    // Process full 64-byte blocks
    size_t full = len & ~(size_t)63u;
    if(full) data = md5_transform(state, data, full);
    size_t rem = len & 63u;

    // Build the padding block(s) in a 128-byte buffer
    uint8_t buf[128];
    memcpy(buf, data, rem);
    buf[rem] = 0x80; // message end "1" bit

    size_t block_len;
    if(rem + 1u <= 56u) {
        // Single block: data + padding + length = 64 bytes
        memset(buf + rem + 1u, 0, 56u - (rem + 1u));
        block_len = 64u;
    } else {
        // Two blocks: first fills 64 bytes, second = zeros + length
        memset(buf + rem + 1u, 0, 64u - (rem + 1u));
        memset(buf + 64u, 0, 56u);
        block_len = 128u;
    }

    // Append the bit length (little-endian 64-bit) at offset 56 of the last block
    uint8_t* lp = buf + block_len - 8u;
    lp[0] = (uint8_t)(bits_lo);
    lp[1] = (uint8_t)(bits_lo >>  8);
    lp[2] = (uint8_t)(bits_lo >> 16);
    lp[3] = (uint8_t)(bits_lo >> 24);
    lp[4] = (uint8_t)(bits_hi);
    lp[5] = (uint8_t)(bits_hi >>  8);
    lp[6] = (uint8_t)(bits_hi >> 16);
    lp[7] = (uint8_t)(bits_hi >> 24);

    md5_transform(state, buf, block_len);

    // Serialize the result (4 x 32-bit words in little-endian)
    for(int i = 0; i < 4; i++) {
        digest[i * 4 + 0] = (uint8_t)(state[i]);
        digest[i * 4 + 1] = (uint8_t)(state[i] >>  8);
        digest[i * 4 + 2] = (uint8_t)(state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(state[i] >> 24);
    }
}
