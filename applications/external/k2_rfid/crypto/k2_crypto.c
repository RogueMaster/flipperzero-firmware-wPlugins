#include "k2_crypto.h"
#include <string.h>

/* u_key: "q3bu^t1nqfZ(pf$1" */
static const uint8_t K2_U_KEY[16] = {
    113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49
};

/* d_key: "H@CFkRnz@KAtBJp2" */
static const uint8_t K2_D_KEY[16] = {
    72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50
};

/* AES-128 Forward and Inverse S-Boxes */
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t rsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint8_t Rcon[11] = {
    0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

typedef struct {
    uint8_t RoundKey[176];
} K2Aes128Ctx;

static void aes_key_expansion(K2Aes128Ctx* ctx, const uint8_t* Key) {
    uint32_t i, j, k;
    uint8_t tempa[4];
    for (i = 0; i < 4; ++i) {
        ctx->RoundKey[(i * 4) + 0] = Key[(i * 4) + 0];
        ctx->RoundKey[(i * 4) + 1] = Key[(i * 4) + 1];
        ctx->RoundKey[(i * 4) + 2] = Key[(i * 4) + 2];
        ctx->RoundKey[(i * 4) + 3] = Key[(i * 4) + 3];
    }
    for (i = 4; i < 44; ++i) {
        k = (i - 1) * 4;
        tempa[0] = ctx->RoundKey[k + 0];
        tempa[1] = ctx->RoundKey[k + 1];
        tempa[2] = ctx->RoundKey[k + 2];
        tempa[3] = ctx->RoundKey[k + 3];

        if (i % 4 == 0) {
            uint8_t u8tmp = tempa[0];
            tempa[0] = tempa[1];
            tempa[1] = tempa[2];
            tempa[2] = tempa[3];
            tempa[3] = u8tmp;

            tempa[0] = sbox[tempa[0]];
            tempa[1] = sbox[tempa[1]];
            tempa[2] = sbox[tempa[2]];
            tempa[3] = sbox[tempa[3]];

            tempa[0] = tempa[0] ^ Rcon[i / 4];
        }
        j = i * 4; k = (i - 4) * 4;
        ctx->RoundKey[j + 0] = ctx->RoundKey[k + 0] ^ tempa[0];
        ctx->RoundKey[j + 1] = ctx->RoundKey[k + 1] ^ tempa[1];
        ctx->RoundKey[j + 2] = ctx->RoundKey[k + 2] ^ tempa[2];
        ctx->RoundKey[j + 3] = ctx->RoundKey[k + 3] ^ tempa[3];
    }
}

static void aes_add_round_key(uint8_t round, uint8_t state[4][4], const uint8_t* RoundKey) {
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[i][j] ^= RoundKey[(round * 16) + (i * 4) + j];
        }
    }
}

static void aes_sub_bytes(uint8_t state[4][4]) {
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[j][i] = sbox[state[j][i]];
        }
    }
}

static void aes_shift_rows(uint8_t state[4][4]) {
    uint8_t temp;
    temp = state[0][1]; state[0][1] = state[1][1]; state[1][1] = state[2][1]; state[2][1] = state[3][1]; state[3][1] = temp;
    temp = state[0][2]; state[0][2] = state[2][2]; state[2][2] = temp;
    temp = state[1][2]; state[1][2] = state[3][2]; state[3][2] = temp;
    temp = state[0][3]; state[0][3] = state[3][3]; state[3][3] = state[2][3]; state[2][3] = state[1][3]; state[1][3] = temp;
}

static inline uint8_t aes_xtime(uint8_t x) {
    return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static void aes_mix_columns(uint8_t state[4][4]) {
    uint8_t Tmp, Tm, t;
    for (uint8_t i = 0; i < 4; ++i) {
        t = state[i][0];
        Tmp = state[i][0] ^ state[i][1] ^ state[i][2] ^ state[i][3];
        Tm = state[i][0] ^ state[i][1]; Tm = aes_xtime(Tm); state[i][0] ^= Tm ^ Tmp;
        Tm = state[i][1] ^ state[i][2]; Tm = aes_xtime(Tm); state[i][1] ^= Tm ^ Tmp;
        Tm = state[i][2] ^ state[i][3]; Tm = aes_xtime(Tm); state[i][2] ^= Tm ^ Tmp;
        Tm = state[i][3] ^ t;           Tm = aes_xtime(Tm); state[i][3] ^= Tm ^ Tmp;
    }
}

static inline uint8_t aes_multiply(uint8_t x, uint8_t y) {
    return (((y & 1) * x) ^
            ((y >> 1 & 1) * aes_xtime(x)) ^
            ((y >> 2 & 1) * aes_xtime(aes_xtime(x))) ^
            ((y >> 3 & 1) * aes_xtime(aes_xtime(aes_xtime(x)))) ^
            ((y >> 4 & 1) * aes_xtime(aes_xtime(aes_xtime(aes_xtime(x))))));
}

static void aes_inv_mix_columns(uint8_t state[4][4]) {
    uint8_t a, b, c, d;
    for (uint8_t i = 0; i < 4; ++i) {
        a = state[i][0]; b = state[i][1]; c = state[i][2]; d = state[i][3];
        state[i][0] = aes_multiply(a, 0x0e) ^ aes_multiply(b, 0x0b) ^ aes_multiply(c, 0x0d) ^ aes_multiply(d, 0x09);
        state[i][1] = aes_multiply(a, 0x09) ^ aes_multiply(b, 0x0e) ^ aes_multiply(c, 0x0b) ^ aes_multiply(d, 0x0d);
        state[i][2] = aes_multiply(a, 0x0d) ^ aes_multiply(b, 0x09) ^ aes_multiply(c, 0x0e) ^ aes_multiply(d, 0x0b);
        state[i][3] = aes_multiply(a, 0x0b) ^ aes_multiply(b, 0x0d) ^ aes_multiply(c, 0x09) ^ aes_multiply(d, 0x0e);
    }
}

static void aes_inv_sub_bytes(uint8_t state[4][4]) {
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[j][i] = rsbox[state[j][i]];
        }
    }
}

static void aes_inv_shift_rows(uint8_t state[4][4]) {
    uint8_t temp;
    temp = state[3][1]; state[3][1] = state[2][1]; state[2][1] = state[1][1]; state[1][1] = state[0][1]; state[0][1] = temp;
    temp = state[0][2]; state[0][2] = state[2][2]; state[2][2] = temp;
    temp = state[1][2]; state[1][2] = state[3][2]; state[3][2] = temp;
    temp = state[0][3]; state[0][3] = state[1][3]; state[1][3] = state[2][3]; state[2][3] = state[3][3]; state[3][3] = temp;
}

static void aes128_ecb_encrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out) {
    K2Aes128Ctx ctx;
    aes_key_expansion(&ctx, key);
    uint8_t state[4][4];
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[i][j] = in[(i * 4) + j];
        }
    }
    aes_add_round_key(0, state, ctx.RoundKey);
    for (uint8_t round = 1; round < 10; ++round) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(round, state, ctx.RoundKey);
    }
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(10, state, ctx.RoundKey);
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            out[(i * 4) + j] = state[i][j];
        }
    }
}

static void aes128_ecb_decrypt_block(const uint8_t* key, const uint8_t* in, uint8_t* out) {
    K2Aes128Ctx ctx;
    aes_key_expansion(&ctx, key);
    uint8_t state[4][4];
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            state[i][j] = in[(i * 4) + j];
        }
    }
    aes_add_round_key(10, state, ctx.RoundKey);
    for (uint8_t round = 9; round > 0; --round) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(round, state, ctx.RoundKey);
        aes_inv_mix_columns(state);
    }
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(0, state, ctx.RoundKey);
    for (uint8_t i = 0; i < 4; ++i) {
        for (uint8_t j = 0; j < 4; ++j) {
            out[(i * 4) + j] = state[i][j];
        }
    }
}

void k2_crypto_derive_key(const uint8_t uid[K2_CRYPTO_UID_SIZE], uint8_t key_out[K2_CRYPTO_KEY_SIZE]) {
    uint8_t uid16[16];
    for (int i = 0; i < 16; i++) {
        uid16[i] = uid[i % K2_CRYPTO_UID_SIZE];
    }
    uint8_t enc_uid[16];
    aes128_ecb_encrypt_block(K2_U_KEY, uid16, enc_uid);
    memcpy(key_out, enc_uid, K2_CRYPTO_KEY_SIZE);
}

void k2_crypto_encrypt_sector1(const uint8_t plain[K2_CRYPTO_SECTOR_SIZE], uint8_t cipher_out[K2_CRYPTO_SECTOR_SIZE]) {
    for (size_t i = 0; i < K2_CRYPTO_SECTOR_SIZE; i += 16) {
        aes128_ecb_encrypt_block(K2_D_KEY, plain + i, cipher_out + i);
    }
}

void k2_crypto_decrypt_sector1(const uint8_t cipher[K2_CRYPTO_SECTOR_SIZE], uint8_t plain_out[K2_CRYPTO_SECTOR_SIZE]) {
    for (size_t i = 0; i < K2_CRYPTO_SECTOR_SIZE; i += 16) {
        aes128_ecb_decrypt_block(K2_D_KEY, cipher + i, plain_out + i);
    }
}

bool k2_crypto_self_test(void) {
    /* Test vector for UID 12 34 56 78 */
    const uint8_t test_uid[4] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t expected_key[6] = {0xf6, 0xa0, 0x67, 0x16, 0xfd, 0x3c};
    uint8_t key[6];
    k2_crypto_derive_key(test_uid, key);
    if (memcmp(key, expected_key, 6) != 0) {
        return false;
    }

    /* Test sector 1 encryption & decryption */
    const uint8_t test_plain[48] = "AB1240276A210100100000FF016500000100000000000000";
    uint8_t cipher[48];
    uint8_t recovered[48];
    k2_crypto_encrypt_sector1(test_plain, cipher);
    k2_crypto_decrypt_sector1(cipher, recovered);
    if (memcmp(test_plain, recovered, 48) != 0) {
        return false;
    }

    return true;
}
