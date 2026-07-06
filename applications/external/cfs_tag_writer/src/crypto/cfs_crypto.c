#include "src/crypto/cfs_crypto.h"

#include <furi.h>
#include <string.h>
#include <mbedtls/aes.h>

#define TAG "CfsCrypto"

// Set to 1 to encrypt/decrypt the 48-byte payload as one chained AES-128-CBC
// run (single zero IV, chained across all 3 blocks). Set to 0 (default) to
// treat each 16-byte block independently with a fresh zero IV -- which is
// mathematically identical to AES-128-ECB per block, matching the literal
// reading of CFS pseudocode found in various reference material online. Flip
// this if on-hardware testing against a real CFS tag shows the ECB
// interpretation produces garbage.
#define CFS_CRYPTO_USE_CHAINED_CBC (0)

// Hardcoded CFS keys, verbatim from various reference material found online.
// Shared across the whole Creality ecosystem (printer firmware + every
// programming tool), so there is no secrecy to protect here.
static const uint8_t cfs_u_key[16] =
    {113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49};
static const uint8_t cfs_d_key[16] =
    {72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50};

// Standard MIFARE transport access bits for the CFS sector-1 trailer: key A/B
// read/write data, key A controls the trailer. Shared by the tag writer and the
// .nfc dump builder (see cfs_crypto.h).
const uint8_t cfs_access_bits[4] = {0xFF, 0x07, 0x80, 0x69};

static void cfs_crypto_run(
    const uint8_t* key,
    int mode,
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    furi_assert(length % CFS_CRYPTO_BLOCK_SIZE == 0);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    if(mode == MBEDTLS_AES_ENCRYPT) {
        mbedtls_aes_setkey_enc(&ctx, key, 128);
    } else {
        mbedtls_aes_setkey_dec(&ctx, key, 128);
    }

#if CFS_CRYPTO_USE_CHAINED_CBC
    uint8_t iv[CFS_CRYPTO_BLOCK_SIZE] = {0};
    mbedtls_aes_crypt_cbc(&ctx, mode, length, iv, input, output);
#else
    // ECB per block == CBC with a fresh zero IV per block, since
    // C = E(P XOR IV) = E(P XOR 0) = E(P) for a single block.
    for(size_t off = 0; off < length; off += CFS_CRYPTO_BLOCK_SIZE) {
        mbedtls_aes_crypt_ecb(&ctx, mode, input + off, output + off);
    }
#endif

    mbedtls_aes_free(&ctx);
}

static void cfs_crypto_derive_key(const uint8_t* uid, size_t uid_len, MfClassicKey* out_key) {
    furi_assert(uid);
    furi_assert(uid_len > 0);
    furi_assert(out_key);

    uint8_t uid16[CFS_CRYPTO_BLOCK_SIZE];
    for(size_t i = 0; i < CFS_CRYPTO_BLOCK_SIZE; i++) {
        uid16[i] = uid[i % uid_len];
    }

    uint8_t result[CFS_CRYPTO_BLOCK_SIZE];
    // Single-block encrypt: ECB and zero-IV CBC are identical here.
    cfs_crypto_run(cfs_u_key, MBEDTLS_AES_ENCRYPT, uid16, result, CFS_CRYPTO_BLOCK_SIZE);

    memcpy(out_key->data, result, MF_CLASSIC_KEY_SIZE);
}

void cfs_crypto_derive_key_a(const uint8_t* uid, size_t uid_len, MfClassicKey* out_key) {
    cfs_crypto_derive_key(uid, uid_len, out_key);
}

void cfs_crypto_derive_key_b(const uint8_t* uid, size_t uid_len, MfClassicKey* out_key) {
    cfs_crypto_derive_key(uid, uid_len, out_key);
}

void cfs_crypto_decrypt(
    const uint8_t ciphertext[CFS_CRYPTO_PLAINTEXT_SIZE],
    uint8_t plaintext[CFS_CRYPTO_PLAINTEXT_SIZE]) {
    cfs_crypto_run(
        cfs_d_key, MBEDTLS_AES_DECRYPT, ciphertext, plaintext, CFS_CRYPTO_PLAINTEXT_SIZE);
}

void cfs_crypto_encrypt(
    const uint8_t plaintext[CFS_CRYPTO_PLAINTEXT_SIZE],
    uint8_t ciphertext[CFS_CRYPTO_PLAINTEXT_SIZE]) {
    cfs_crypto_run(
        cfs_d_key, MBEDTLS_AES_ENCRYPT, plaintext, ciphertext, CFS_CRYPTO_PLAINTEXT_SIZE);
}

bool cfs_crypto_self_test(void) {
    // Reconstructed from a CFS example in reference material found online, laid out per the
    // byte-offset table (date[5] vendor[4] batch[2] filament[6] color[7]
    // length[4] serial[6] reserved[14]).
    static const char sample[CFS_CRYPTO_PLAINTEXT_SIZE] = {
        'A', 'B', 'C', '2', '1', // date
        '0', '2', '7', '6', //       vendor
        '0', '1', //                 batch
        '1', '0', '1', '0', '0', '1', // filament id
        '0', 'F', 'F', '5', 'F', '0', 'B', // color
        '0', '1', '6', '5', //       length
        '7', '3', '6', '3', '1', '4', // serial
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // reserved
    };

    uint8_t cipher[CFS_CRYPTO_PLAINTEXT_SIZE];
    uint8_t plain[CFS_CRYPTO_PLAINTEXT_SIZE];

    cfs_crypto_encrypt((const uint8_t*)sample, cipher);
    cfs_crypto_decrypt(cipher, plain);

    bool ok = memcmp(sample, plain, CFS_CRYPTO_PLAINTEXT_SIZE) == 0;

    if(ok) {
        FURI_LOG_I(
            TAG,
            "Self-test PASS (encrypt->decrypt round trip). NOTE: round-trip only, "
            "not validated against real captured ciphertext.");
    } else {
        FURI_LOG_E(TAG, "Self-test FAIL: round trip did not match");
    }

    // UID key-derivation smoke check using the spec's example UID 60 EA 12 21.
    const uint8_t uid[4] = {0x60, 0xEA, 0x12, 0x21};
    MfClassicKey key;
    cfs_crypto_derive_key_a(uid, sizeof(uid), &key);
    FURI_LOG_I(
        TAG,
        "Derived KeyA for UID 60EA1221: %02X %02X %02X %02X %02X %02X",
        key.data[0],
        key.data[1],
        key.data[2],
        key.data[3],
        key.data[4],
        key.data[5]);

    return ok;
}
