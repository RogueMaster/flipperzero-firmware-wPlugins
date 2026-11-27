#include "flipbip_string.h"
#include <ctype.h>
#include <stdint.h>
#include <string.h>
// From: lib/crypto
#include <memzero.h>
#include <rc4.h>

void flipbip_btox(const unsigned char* in, int in_len, char* str) {
    for(int i = 0; i < in_len; i++) {
        unsigned char n;
        unsigned char x = in[i];

        str += 2;
        *(str + (i * 2)) = '\0';

        for(n = 2; n != 0; --n) {
            *(--str + (i * 2)) = "0123456789abcdef"[x & 0x0F];
            x >>= 4;
        }
    }
}
void flipbip_xtob(const char* str, unsigned char* out, int out_len) {
    int len = strlen(str) / 2;
    if(len > out_len) len = out_len;
    for(int i = 0; i < len; i++) {
        char c = 0;
        if(str[i * 2] >= '0' && str[i * 2] <= '9') c += (str[i * 2] - '0') << 4;
        if((str[i * 2] & ~0x20) >= 'A' && (str[i * 2] & ~0x20) <= 'F')
            c += (10 + (str[i * 2] & ~0x20) - 'A') << 4;
        if(str[i * 2 + 1] >= '0' && str[i * 2 + 1] <= '9') c += (str[i * 2 + 1] - '0');
        if((str[i * 2 + 1] & ~0x20) >= 'A' && (str[i * 2 + 1] & ~0x20) <= 'F')
            c += (10 + (str[i * 2 + 1] & ~0x20) - 'A');
        out[i] = c;
    }
}

void flipbip_cipher(
    const unsigned char* key_in,
    const unsigned int key_len,
    const char* in,
    char* out,
    const unsigned int io_len) {
    if(io_len > 512) return;

    RC4_CTX ctx;
    uint8_t buf[256];
    memzero(buf, 256);

    flipbip_xtob(in, buf, io_len / 2);

    rc4_init(&ctx, key_in, key_len);
    rc4_encrypt(&ctx, buf, 256);

    flipbip_btox(buf, io_len / 2, out);

    memzero(buf, 256);
}
