#include "sha1.h"
#include <stdio.h>
#include <string.h>

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))

static void sha1_transform(Sha1Context* ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, k, temp, m[80];
    for(int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (data[j] << 24) | (data[j+1] << 16) | (data[j+2] << 8) | (data[j+3]);
    }
    
    for(int i = 16; i < 80; ++i) {
        m[i] = ROTLEFT(m[i-3] ^ m[i-8] ^ m[i-14] ^ m[i-16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for(int i = 0; i < 80; ++i) {
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        temp = ROTLEFT(a, 5) + f + e + k + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void sha1_init(Sha1Context* ctx) {
    ctx->count = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
}

void sha1_update(Sha1Context* ctx, const uint8_t* data, size_t len) {
    size_t i = ctx->count % 64;
    ctx->count += len;
    size_t fill = 64 - i;

    if(i && len >= fill) {
        memcpy(ctx->buffer + i, data, fill);
        sha1_transform(ctx, ctx->buffer);
        data += fill;
        len -= fill;
        i = 0;
    }

    while(len >= 64) {
        sha1_transform(ctx, data);
        data += 64;
        len -= 64;
    }

    if(len > 0)
        memcpy(ctx->buffer + i, data, len);
}

void sha1_finalize(Sha1Context* ctx, uint8_t hash[20]) {
    uint8_t pad[64] = {0x80};
    uint64_t bits = ctx->count * 8;
    uint8_t len_pad[8];

    for(int i = 0; i < 8; i++)
        len_pad[7 - i] = bits >> (i * 8);

    size_t pad_len = (ctx->count % 64 < 56) ? 56 - (ctx->count % 64) : 120 - (ctx->count % 64);
    sha1_update(ctx, pad, pad_len);
    sha1_update(ctx, len_pad, 8);

    for(int i = 0; i < 5; i++) {
        hash[i*4] = (ctx->state[i] >> 24) & 0xff;
        hash[i*4+1] = (ctx->state[i] >> 16) & 0xff;
        hash[i*4+2] = (ctx->state[i] >> 8) & 0xff;
        hash[i*4+3] = ctx->state[i] & 0xff;
    }
}

void sha1_to_hex(const uint8_t hash[20], char* output) {
    for (int i = 0; i < 20; ++i) {
        snprintf(output + i * 2, 3, "%02x", hash[i]);
    }
    output[40] = '\0';
}