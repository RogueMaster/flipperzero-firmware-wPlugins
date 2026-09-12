#include "cipher_registry.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciphers/adfgvx.h"
#include "ciphers/aes.h"
#include "ciphers/affine.h"
#include "ciphers/atbash.h"
#include "ciphers/baconian.h"
#include "ciphers/beaufort.h"
#include "ciphers/bifid.h"
#include "ciphers/caesar.h"
#include "ciphers/playfair.h"
#include "ciphers/polybius.h"
#include "ciphers/porta.h"
#include "ciphers/railfence.h"
#include "ciphers/rc4.h"
#include "ciphers/rot13.h"
#include "ciphers/scytale.h"
#include "ciphers/trifid.h"
#include "ciphers/vigenere.h"

#include "hashes/blake2.h"
#include "hashes/fnv.h"
#include "hashes/md2.h"
#include "hashes/md5.h"
#include "hashes/murmur3.h"
#include "hashes/sha1.h"
#include "hashes/sha2.h"
#include "hashes/siphash.h"
#include "hashes/xxhash.h"

#include "encoders/base16.h"
#include "encoders/base32.h"
#include "encoders/base58.h"
#include "encoders/base64.h"

static CipherResult ok_result(char* text) {
    return (CipherResult){.ok = true, .text = text};
}

static CipherResult err_result(const char* message) {
    return (CipherResult){.ok = false, .text = strdup(message)};
}

void cipher_result_free(CipherResult* result) {
    if(!result) return;
    free(result->text);
    result->text = NULL;
}

static const char kHexChars[] = "0123456789abcdef";

static char* bytes_to_hex(const uint8_t* bytes, size_t len) {
    char* out = malloc(len * 2 + 1);
    furi_assert(out);
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = kHexChars[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHexChars[bytes[i] & 0xF];
    }
    out[len * 2] = '\0';
    return out;
}

#define SIMPLE_TRANSFORM_WRAPPER(fn_name, underlying_fn) \
    static CipherResult fn_name(const char* input, int32_t a, int32_t b, const char* k) { \
        UNUSED(a); \
        UNUSED(b); \
        UNUSED(k); \
        return ok_result(strdup(underlying_fn((char*)input))); \
    }

static CipherResult adfgvx_encode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(adfgvx_encrypt(input, key));
}

static CipherResult adfgvx_decode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(adfgvx_decrypt(input, key));
}

static CipherResult aes_encode(const char* input, int32_t a, int32_t b, const char* text_key) {
    UNUSED(a);
    UNUSED(b);
    if(!text_key || strlen(text_key) != 16) {
        return err_result("Key must be 16 bytes");
    }
    size_t len = strlen(input);
    if(len > 128) len = 128;

    uint8_t key[16];
    memcpy(key, text_key, 16);
    uint8_t iv[16] = {0};
    uint8_t buf[128];
    memcpy(buf, input, len);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, buf, len);

    char* out = malloc(2 * 128 + 1);
    furi_assert(out);
    aes_bytes_to_hex(buf, len, out);
    return ok_result(out);
}

static CipherResult aes_decode(const char* input, int32_t a, int32_t b, const char* text_key) {
    UNUSED(a);
    UNUSED(b);
    if(!text_key || strlen(text_key) != 16) {
        return err_result("Key must be 16 bytes");
    }
    size_t input_len = strlen(input);
    if(input_len > 256) input_len = 256;
    if(input_len % 2 != 0) {
        return err_result("Invalid hex length");
    }
    size_t len = input_len / 2;

    uint8_t key[16];
    memcpy(key, text_key, 16);
    uint8_t iv[16] = {0};
    uint8_t buf[128];
    aes_hex_to_bytes(input, buf, input_len);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CTR_xcrypt_buffer(&ctx, buf, len);

    char* out = malloc(len + 1);
    furi_assert(out);
    memcpy(out, buf, len);
    out[len] = '\0';
    return ok_result(out);
}

static bool affine_key_a_valid(int32_t a) {
    return (a % 2 != 0) && (a != 13);
}

static CipherResult affine_encode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(k);
    if(!affine_key_a_valid(a)) return err_result("Key A must be odd, not 13");
    return ok_result(strdup(encode_affine((char*)input, a, b)));
}

static CipherResult affine_decode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(k);
    if(!affine_key_a_valid(a)) return err_result("Key A must be odd, not 13");
    return ok_result(strdup(decode_affine((char*)input, a, b)));
}

SIMPLE_TRANSFORM_WRAPPER(atbash_transform, atbash_encrypt_or_decrypt) // self-inverse
SIMPLE_TRANSFORM_WRAPPER(baconian_encode_wrap, baconian_encrypt)
SIMPLE_TRANSFORM_WRAPPER(baconian_decode_wrap, baconian_decrypt)
SIMPLE_TRANSFORM_WRAPPER(polybius_encode_wrap, encrypt_polybius)
SIMPLE_TRANSFORM_WRAPPER(polybius_decode_wrap, decrypt_polybius)
SIMPLE_TRANSFORM_WRAPPER(rot13_encode_wrap, encrypt_rot13)
SIMPLE_TRANSFORM_WRAPPER(rot13_decode_wrap, decrypt_rot13)

static CipherResult beaufort_transform(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(beaufort_cipher_encrypt_and_decrypt((char*)input, (char*)key)));
}

static CipherResult bifid_encode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(bifid_encrypt(input, key));
}

static CipherResult bifid_decode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(bifid_decrypt(input, key));
}

static CipherResult porta_transform(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(porta_encrypt_and_decrypt((char*)input, (char*)key)));
}

static CipherResult caesar_encode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(encode_caesar((char*)input, a)));
}

static CipherResult caesar_decode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(decode_caesar((char*)input, a)));
}

static CipherResult playfair_encode(const char* input, int32_t a, int32_t b, const char* keyword) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(playfair_encrypt((char*)input, playfair_make_table((char*)keyword))));
}

static CipherResult playfair_decode(const char* input, int32_t a, int32_t b, const char* keyword) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(playfair_decrypt((char*)input, playfair_make_table((char*)keyword))));
}

static CipherResult railfence_encode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(rail_fence_encrypt((char*)input, a)));
}

static CipherResult railfence_decode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(rail_fence_decrypt((char*)input, a)));
}

static CipherResult scytale_encode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(scytale_encrypt((char*)input, a)));
}

static CipherResult scytale_decode(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(scytale_decrypt((char*)input, a)));
}

static CipherResult trifid_encode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(trifid_encrypt(input, key));
}

static CipherResult trifid_decode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(trifid_decrypt(input, key));
}

static CipherResult rc4_encode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    size_t input_len = strlen(input);
    unsigned char* encrypted =
        rc4_encrypt_and_decrypt((char*)key, (const unsigned char*)input, input_len);
    if(!encrypted) return err_result("Encryption failed");

    char* hex = rc4_to_hex((const char*)encrypted, input_len);
    char* out = strdup(hex);
    free(hex);
    free(encrypted);
    return ok_result(out);
}

static CipherResult rc4_decode(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    size_t encrypted_len;
    unsigned char* encrypted_bytes = rc4_hex_to_bytes((char*)input, &encrypted_len);
    if(!encrypted_bytes) return err_result("Invalid hex input");

    unsigned char* decrypted = rc4_encrypt_and_decrypt((char*)key, encrypted_bytes, encrypted_len);
    free(encrypted_bytes);
    if(!decrypted) return err_result("Decryption failed");

    char* out = malloc(encrypted_len + 1);
    furi_assert(out);
    memcpy(out, decrypted, encrypted_len);
    out[encrypted_len] = '\0';
    free(decrypted);
    return ok_result(out);
}

static CipherResult vigenere_encode(const char* input, int32_t a, int32_t b, const char* keyword) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(vigenere_encrypt((char*)input, (char*)keyword)));
}

static CipherResult vigenere_decode(const char* input, int32_t a, int32_t b, const char* keyword) {
    UNUSED(a);
    UNUSED(b);
    return ok_result(strdup(vigenere_decrypt((char*)input, (char*)keyword)));
}

static CipherResult blake2_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    Blake2sContext ctx;
    uint8_t hash[BLAKE2S_OUTLEN];
    char hex[BLAKE2S_OUTLEN * 2 + 1] = {0};
    blake2s_init(&ctx);
    blake2s_update(&ctx, (const uint8_t*)input, strlen(input));
    blake2s_finalize(&ctx, hash);
    blake2s_to_hex(hash, hex);
    return ok_result(strdup(hex));
}

static CipherResult fnv1a_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    char str[11];
    Fnv32_t hash = fnv_32a_str((char*)input, FNV1_32A_INIT);
    snprintf(str, sizeof(str), "0x%08lx", (unsigned long)hash);
    return ok_result(strdup(str));
}

static CipherResult md2_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    BYTE hash[MD2_BLOCK_SIZE];
    MD2_CTX ctx;
    md2_init(&ctx);
    md2_update(&ctx, (const BYTE*)input, strlen(input));
    md2_final(&ctx, hash);
    return ok_result(bytes_to_hex(hash, MD2_BLOCK_SIZE));
}

static CipherResult md5_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint8_t hash[16];
    char hex[33] = {0};
    MD5Context ctx;
    md5_init(&ctx);
    md5_update(&ctx, (const uint8_t*)input, strlen(input));
    md5_finalize(&ctx, hash);
    md5_to_hex(hash, hex);
    return ok_result(strdup(hex));
}

static CipherResult murmur3_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(MurmurHash3_x86_32((char*)input, strlen(input), 0)));
}

static CipherResult sip_hash(const char* input, int32_t a, int32_t b, const char* key) {
    UNUSED(a);
    UNUSED(b);
    if(!key || strlen(key) != 16) {
        return err_result("Key must be 16 chars");
    }
    uint8_t output[8];
    siphash((char*)input, strlen(input), (char*)key, output, 8);
    return ok_result(bytes_to_hex(output, 8));
}

static CipherResult sha1_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    Sha1Context ctx;
    uint8_t hash[20];
    char hex[41] = {0};
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t*)input, strlen(input));
    sha1_finalize(&ctx, hash);
    sha1_to_hex(hash, hex);
    return ok_result(strdup(hex));
}

static CipherResult sha224_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint8_t hash[28];
    sha224((const uint8_t*)input, (uint64)strlen(input), hash);
    return ok_result(bytes_to_hex(hash, 28));
}

static CipherResult sha256_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint8_t hash[32];
    sha256((const uint8_t*)input, (uint64)strlen(input), hash);
    return ok_result(bytes_to_hex(hash, 32));
}

static CipherResult sha384_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint8_t hash[48];
    sha384((const uint8_t*)input, (uint64)strlen(input), hash);
    return ok_result(bytes_to_hex(hash, 48));
}

static CipherResult sha512_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint8_t hash[64];
    sha512((const uint8_t*)input, (uint64)strlen(input), hash);
    return ok_result(bytes_to_hex(hash, 64));
}

static CipherResult xxhash_hash(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    uint64_t hash = XXH64((char*)input, strlen(input), 0);
    char str[17];
    snprintf(str, sizeof(str), "%016llX", (unsigned long long)hash);
    return ok_result(strdup(str));
}

static CipherResult base16_encode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(base16_encode(input)));
}

static CipherResult base16_decode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    size_t out_len;
    return ok_result(strdup(base16_decode(input, &out_len)));
}

static CipherResult base32_encode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(base32_encode((const uint8_t*)input, strlen(input))));
}

static CipherResult base32_decode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    size_t out_len;
    return ok_result(strdup((const char*)base32_decode((char*)input, &out_len)));
}

static CipherResult base58_encode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(base58_encode((char*)input)));
}

static CipherResult base58_decode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(base58_decode((char*)input)));
}

static CipherResult base64_encode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    return ok_result(strdup(base64_encode((const unsigned char*)input, strlen(input))));
}

static CipherResult base64_decode_wrap(const char* input, int32_t a, int32_t b, const char* k) {
    UNUSED(a);
    UNUSED(b);
    UNUSED(k);
    size_t out_len;
    return ok_result(strdup((const char*)base64_decode((char*)input, &out_len)));
}

const CipherDef kCiphers[] = {
    {
        .name = "ADFGVX Cipher",
        .file_key = "adfgvx",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = adfgvx_encode,
        .decode = adfgvx_decode,
        .key_a_prompt = "Enter keyword",
        .learn_text =
            "The ADFGVX cipher was used by the German Army during World War I to secure "
            "radio communications. It works in two stages: first, each letter and digit is "
            "converted into a pair of letters from the set A, D, F, G, V, X using a 6x6 grid, "
            "a process called fractionation. Second, the resulting stream of letters is "
            "rearranged using a keyed columnar transposition, scrambling the order based on "
            "a secret keyword. The combination of substitution and transposition made ADFGVX "
            "considerably harder to break than earlier field ciphers, though French "
            "cryptanalyst Georges Painvin famously broke it during the war, a notable early "
            "victory in modern cryptanalysis.",
    },
    {
        .name = "AES-128 Cipher",
        .file_key = "aes",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = aes_encode,
        .decode = aes_decode,
        .key_a_prompt = "Enter key (sixteen chars)",
        .text_key_exact_len = 16,
        .learn_text =
            "AES-128 (Advanced Encryption Standard with a 128-bit key) is a symmetric block cipher "
            "widely used for data encryption. It encrypts data in fixed blocks of 128 bits using a "
            "128-bit key and operates through 10 rounds of transformations, including substitution, "
            "permutation, mixing, and key addition. AES-128 is known for its strong security and "
            "efficiency, and is a standard for protecting sensitive data in everything from "
            "government communications to online banking. Unlike classical ciphers, AES relies on "
            "complex mathematical operations and is resistant to all known practical cryptographic "
            "attacks when implemented properly.",
    },
    {
        .name = "Affine Cipher",
        .file_key = "affine",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNumberDouble,
        .encode = affine_encode,
        .decode = affine_decode,
        .key_a_min = 1, .key_a_max = 25,
        .key_b_min = 1, .key_b_max = 30,
        .key_a_prompt = "Odd # between 1-25, not 13",
        .key_b_prompt = "Enter key (1 - 30)",
        .learn_text =
            "The Affine cipher is a type of monoalphabetic substitution cipher that uses a "
            "mathematical formula to encrypt each letter: E(x) = (a x x + b) mod 26, where x is the "
            "position of the plaintext letter in the alphabet (A = 0, B = 1, etc.), and a and b are "
            "keys. The value of a must be coprime with 26 to ensure that each letter maps uniquely. "
            "Decryption uses the inverse of a with the formula D(x) = a^-1 x (x - b) mod 26. The "
            "Affine cipher combines multiplicative and additive shifts, making it slightly more "
            "secure than a Caesar cipher, but still vulnerable to frequency analysis.",
    },
    {
        .name = "Atbash Cipher",
        .file_key = "atbash",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNone,
        .encode = atbash_transform,
        .decode = atbash_transform,
        .learn_text =
            "The Atbash cipher is a classical substitution cipher originally used for the Hebrew "
            "alphabet. It works by reversing the alphabet so that the first letter becomes the "
            "last, the second becomes the second-to-last, and so on. For example, in the Latin "
            "alphabet, 'A' becomes 'Z', 'B' becomes 'Y', and 'C' becomes 'X'. It is a simple and "
            "symmetric cipher, meaning that the same algorithm is used for both encryption and "
            "decryption. Though not secure by modern standards, the Atbash cipher is often studied "
            "for its historical significance.",
    },
    {
        .name = "Baconian Cipher",
        .file_key = "baconian",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNone,
        .encode = baconian_encode_wrap,
        .decode = baconian_decode_wrap,
        .learn_text =
            "The Baconian cipher, developed by Francis Bacon in the early 17th century, is a "
            "steganographic cipher that encodes each letter of the alphabet into a series of five "
            "binary characters, typically using combinations of 'A' and 'B'. For example, the "
            "letter 'A' is represented as 'AAAAA', 'B' as 'AAAAB', and so on. This binary code can "
            "then be hidden within text, images, or formatting, making it a method of concealed "
            "rather than encrypted communication. The Baconian cipher is notable for being an "
            "early example of steganography and is often used in historical or educational "
            "contexts.",
    },
    {
        .name = "Beaufort Cipher",
        .file_key = "beaufort",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = beaufort_transform,
        .decode = beaufort_transform,
        .key_a_prompt = "Enter key",
        .learn_text =
            "The Beaufort cipher is a polyalphabetic substitution cipher that is similar to the "
            "Vigenere cipher but uses a slightly different encryption algorithm. Instead of adding "
            "the key to the plaintext, it subtracts the plaintext letter from the key letter using "
            "a tabula recta, meaning that the same process is used for both encryption and "
            "decryption. The cipher was named after Sir Francis Beaufort and was historically used "
            "in applications like encrypting naval signals. While more secure than simple ciphers "
            "like Caesar, it is still vulnerable to modern cryptanalysis techniques.",
    },
    {
        .name = "Bifid Cipher",
        .file_key = "bifid",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = bifid_encode,
        .decode = bifid_decode,
        .key_a_prompt = "Enter keyword",
        .learn_text =
            "The Bifid cipher, invented around 1901 by French cryptographer Felix Delastelle, "
            "combines a Polybius square with a fractionation and transposition step to "
            "encrypt letters based on both their position and the position of surrounding "
            "letters. Each letter is first converted to a pair of coordinates using a 5x5 "
            "grid (with I and J sharing a cell). All the row coordinates are then written "
            "out, followed by all the column coordinates, and this combined sequence is read "
            "off in new pairs and converted back into letters using the same square. Because "
            "each ciphertext letter depends on the coordinates of two different plaintext "
            "letters, Bifid diffuses information across the message more than simple "
            "substitution ciphers, making frequency analysis considerably harder.",
    },
    {
        .name = "Caesar Cipher",
        .file_key = "caesar",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNumberSingle,
        .encode = caesar_encode,
        .decode = caesar_decode,
        .key_a_min = 1, .key_a_max = 26,
        .key_a_prompt = "Enter key (1 - 26)",
        .learn_text =
            "The Caesar cipher is a simple and well-known substitution cipher named after Julius "
            "Caesar, who reportedly used it to protect military messages. It works by shifting "
            "each letter in the plaintext a fixed number of positions down the alphabet. For "
            "example, with a shift of 3, 'A' becomes 'D', 'B' becomes 'E', and so on. After 'Z', "
            "the cipher wraps around to the beginning of the alphabet. While easy to understand "
            "and implement, the Caesar cipher is also extremely easy to break.",
    },
    {
        .name = "Playfair Cipher",
        .file_key = "playfair",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = playfair_encode,
        .decode = playfair_decode,
        .key_a_prompt = "Enter key",
        .learn_text =
            "The Playfair cipher is a manual symmetric encryption technique invented in 1854 by "
            "Charles Wheatstone but popularized by Lord Playfair. It encrypts pairs of letters "
            "(digraphs) instead of single letters, making it more secure than simple substitution "
            "ciphers. The cipher uses a 5x5 grid of letters constructed from a keyword, combining "
            "'I' and 'J' to fit the alphabet into 25 cells. To encrypt, each pair of letters is "
            "located in the grid, and various rules are applied based on their positions like same "
            "row, same column, or rectangle to substitute them with new letters. The Playfair "
            "cipher was used historically for military communication due to its relative ease of "
            "use and stronger encryption for its time.",
    },
    {
        .name = "Polybius Square",
        .file_key = "polybius",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNone,
        .encode = polybius_encode_wrap,
        .decode = polybius_decode_wrap,
        .learn_text =
            "The Polybius square is a classical cipher that uses a 5x5 grid filled with letters of "
            "the alphabet to convert plaintext into pairs of numbers. Each letter is identified by "
            "its row and column numbers in the grid. For example, 'A' might be encoded as '11', "
            "'B' as '12', and so on. Since the Latin alphabet has 26 letters, 'I' and 'J' are "
            "typically combined to fit into the 25-cell grid. The Polybius square is easy to "
            "implement and was historically used for signaling and cryptography in wartime. It is "
            "simple and easy to decode, and therefore offers minimal security by modern standards.",
    },
    {
        .name = "Porta Cipher",
        .file_key = "porta",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = porta_transform,
        .decode = porta_transform,
        .key_a_prompt = "Enter key",
        .learn_text =
            "The Porta cipher is a classical polyalphabetic substitution cipher invented by "
            "Giovanni Battista della Porta in the 16th century. Unlike simple monoalphabetic "
            "ciphers, it uses a repeating key to select from a set of 13 reciprocal substitution "
            "alphabets, where each pair of key letters (A/B, C/D, etc.) corresponds to one "
            "alphabet. Because the substitution is reciprocal, the same process is used for both "
            "encryption and decryption, making it relatively easy to implement. While more secure "
            "than a Caesar cipher, it is still vulnerable to frequency analysis and modern "
            "cryptanalysis techniques.",
    },
    {
        .name = "Railfence Cipher",
        .file_key = "railfence",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNumberSingle,
        .encode = railfence_encode,
        .decode = railfence_decode,
        .key_a_min = 1, .key_a_max = 8,
        .key_a_prompt = "Enter key (1 - 8)",
        .learn_text =
            "The Rail Fence cipher is a form of transposition cipher that rearranges the letters "
            "of the plaintext in a zigzag pattern across multiple 'rails' (or rows), and then "
            "reads them off row by row to create the ciphertext. For example, using 3 rails, the "
            "message 'HELLO WORLD' would be written in a zigzag across three lines and then read "
            "horizontally to produce the encrypted message. It's a simple method that relies on "
            "obscuring the letter order rather than substituting characters, and it's relatively "
            "easy to decrypt with enough trial and error.",
    },
    {
        .name = "RC4 Cipher",
        .file_key = "rc4",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = rc4_encode,
        .decode = rc4_decode,
        .key_a_prompt = "Enter key",
        .learn_text =
            "RC4 is a stream cipher designed by Ron Rivest in 1987 and widely used for its speed "
            "and simplicity. It generates a pseudorandom keystream that is XORed with the "
            "plaintext to produce ciphertext. RC4's internal state consists of a 256-byte array "
            "and a pair of index pointers, updated in a key-dependent manner. While once popular "
            "in protocols like SSL and WEP, RC4 has been found to have significant "
            "vulnerabilities, especially related to key scheduling, and is now considered "
            "insecure for modern uses.",
    },
    {
        .name = "ROT-13 Cipher",
        .file_key = "rot13",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNone,
        .encode = rot13_encode_wrap,
        .decode = rot13_decode_wrap,
        .learn_text =
            "ROT13 (short for 'rotate by 13 places') is a simple letter substitution cipher used "
            "primarily to obscure text rather than securely encrypt it. It works by shifting each "
            "letter of the alphabet 13 positions forward, wrapping around from Z back to A if "
            "necessary. Because the alphabet has 26 letters, applying ROT13 twice returns the "
            "original text, making it a symmetric cipher. ROT13 is commonly used in online forums "
            "to hide spoilers, puzzles, or offensive content, but it offers no real security and "
            "can be easily reversed without a key.",
    },
    {
        .name = "Scytale Cipher",
        .file_key = "scytale",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyNumberSingle,
        .encode = scytale_encode,
        .decode = scytale_decode,
        .key_a_min = 1, .key_a_max = 9,
        .key_a_prompt = "Enter key (1 - 9)",
        .learn_text =
            "The Scytale cipher is an ancient transposition cipher used by the Spartans. It "
            "involves wrapping a strip of parchment around a rod (scytale) of a fixed diameter "
            "and writing the message along the rod's surface. When unwrapped, the text appears "
            "scrambled unless it is rewrapped around a rod of the same size. The security relies "
            "on the secrecy of the rod's diameter. Although simple and easy to use, the Scytale "
            "cipher offers almost no security by modern standards and just of historical "
            "interest.",
    },
    {
        .name = "Trifid Cipher",
        .file_key = "trifid",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = trifid_encode,
        .decode = trifid_decode,
        .key_a_prompt = "Enter keyword",
        .learn_text =
            "The Trifid cipher, invented by Felix Delastelle in 1901 as an extension of "
            "his earlier Bifid cipher, plots letters into a 3x3x3 cube of 27 cells (the 26 "
            "letters plus a period) instead of a flat 5x5 square. Each letter is converted "
            "into three coordinates: layer, row, and column. All the layer values are written "
            "out first, then all the row values, then all the column values, and this "
            "combined sequence is read off in new groups of three and converted back into "
            "letters using the cube. Because each ciphertext letter can depend on the "
            "coordinates of up to three different plaintext letters, Trifid spreads "
            "information even further than Bifid, making it noticeably more resistant to "
            "frequency analysis while still doable by hand.",
    },
    {
        .name = "Vigenere Cipher",
        .file_key = "vigenere",
        .category = CipherCategoryCipher,
        .key_kind = CipherKeyText,
        .encode = vigenere_encode,
        .decode = vigenere_decode,
        .key_a_prompt = "Enter key",
        .learn_text =
            "The Vigenere cipher is a classical polyalphabetic substitution cipher that uses a "
            "keyword to determine the shift for each letter of the plaintext. Each letter of the "
            "keyword corresponds to a Caesar cipher shift, which is applied cyclically over the "
            "plaintext. For example, with the keyword 'KEY', the first letter of the plaintext is "
            "shifted by the position of 'K', the second by 'E', and so on. This method makes "
            "frequency analysis more difficult than in simple substitution ciphers, and it was "
            "considered unbreakable for centuries until modern cryptanalysis techniques were "
            "developed.",
    },
    {
        .name = "BLAKE-2s Hash",
        .file_key = "blake2",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = blake2_hash,
        .decode = NULL,
        .learn_text =
            "BLAKE-2s is a high performance cryptographic hash function designed as an improved "
            "alternative to MD5, SHA-1, and even SHA-2, offering strong security with faster "
            "hashing speeds. Developed by Jean-Philippe Aumasson and others, it builds on the "
            "cryptographic foundations of the BLAKE algorithm (a finalist in the SHA-3 "
            "competition) but is optimized for practical use. BLAKE2 comes in two main variants: "
            "BLAKE2b (optimized for 64-bit platforms) and BLAKE2s (for 8- to 32-bit platforms). "
            "It provides features like keyed hashing, salting, and personalization, making it "
            "suitable for applications like password hashing, digital signatures, and message "
            "authentication. BLAKE2 is widely adopted due to its balance of speed, simplicity, "
            "and security, and is used in software like Argon2 (a password hashing algorithm) "
            "and various blockchain projects.",
    },
    {
        .name = "FNV-1A Hash",
        .file_key = "fnv1a",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = fnv1a_hash,
        .decode = NULL,
        .learn_text =
            "FNV-1a (Fowler-Noll-Vo hash, variant 1a) is a simple, fast, and widely used "
            "non-cryptographic hash function designed for use in hash tables and data indexing. "
            "It works by starting with a fixed offset basis, then for each byte of input, it "
            "XORs the byte with the hash and multiplies the result by a prime number (commonly "
            "16777619 for 32-bit or 1099511628211 for 64-bit). FNV-1a is known for its good "
            "distribution and performance on small inputs, but it's not cryptographically secure "
            "and should not be used for security-sensitive applications. Its simplicity and "
            "efficiency make it a favorite in performance-critical systems and embedded "
            "environments.",
    },
    {
        .name = "MD2 Hash",
        .file_key = "md2",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = md2_hash,
        .decode = NULL,
        .learn_text =
            "MD2 (Message Digest 2) is a cryptographic hash function designed by Ronald Rivest in "
            "1989. It produces a 128-bit (16-byte) hash value from an input of any length, "
            "typically used to verify data integrity. Although it was once widely used, MD2 is "
            "now considered obsolete due to its slow performance and vulnerabilities to collision "
            "attacks. As a result, more secure and efficient hash functions like SHA-2 or SHA-3 "
            "are recommended for modern applications. Despite its weaknesses, MD2 remains an "
            "important part of cryptographic history and legacy systems.",
    },
    {
        .name = "MD5 Hash",
        .file_key = "md5",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = md5_hash,
        .decode = NULL,
        .learn_text =
            "MD5 (Message Digest Algorithm 5) is a widely used cryptographic hash function that "
            "produces a 128-bit (16 byte) hash value, typically rendered as a 32-character "
            "hexadecimal number. Originally designed for digital signatures and file integrity "
            "verification, MD5 is now considered cryptographically broken due to known collision "
            "vulnerabilities. While still used in some non-security-critical contexts, it is not "
            "recommended for new cryptographic applications.",
    },
    {
        .name = "MurmurHash3",
        .file_key = "murmur3",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = murmur3_hash,
        .decode = NULL,
        .learn_text =
            "MurmurHash3 is a non-cryptographic hash function designed for fast hashing "
            "performance, primarily used in hash-based data structures like hash tables and "
            "bloom filters. It was developed by Austin Appleby and is the third and final "
            "version of the MurmurHash family. MurmurHash3 offers excellent distribution and low "
            "collision rates for general-purpose use, with versions optimized for both 32-bit "
            "and 128-bit outputs. Its speed and simplicity make it a popular choice in software "
            "like databases, compilers, and networking tools. However, it is not suitable for "
            "cryptographic purposes because it lacks the security properties needed to resist "
            "things like collision or preimage attacks.",
    },
    {
        .name = "SipHash",
        .file_key = "sip",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyText,
        .encode = sip_hash,
        .decode = NULL,
        .key_a_prompt = "Enter 16 char keyword",
        .text_key_exact_len = 16,
        .learn_text =
            "SipHash is a fast, cryptographically secure hash function designed specifically to "
            "protect hash tables from DoS attacks caused by hash collisions. Developed by "
            "Jean-Philippe Aumasson and Daniel J. Bernstein, SipHash uses a secret key to produce "
            "a 64-bit (or 128-bit) hash, making it resistant to hash-flooding attacks where an "
            "attacker intentionally causes many collisions. While not as fast as "
            "non-cryptographic hashes like MurmurHash, it strikes a balance between speed and "
            "security, making it ideal for situations where untrusted input needs to be safely "
            "hashed, such as in web servers and language runtimes.",
    },
    {
        .name = "SHA-1 Hash",
        .file_key = "sha1",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = sha1_hash,
        .decode = NULL,
        .learn_text =
            "SHA-1 (Secure Hash Algorithm 1) is a cryptographic hash function that produces a "
            "160-bit (20-byte) hash value. Once widely used in SSL certificates and digital "
            "signatures, SHA-1 has been deprecated due to demonstrated collision attacks, where "
            "two different inputs produce the same hash. As a result, it's no longer considered "
            "secure for cryptographic purposes and has largely been replaced by stronger "
            "alternatives.",
    },
    {
        .name = "SHA-224 Hash",
        .file_key = "sha224",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = sha224_hash,
        .decode = NULL,
        .learn_text =
            "SHA-224 is a cryptographic hash function from the SHA-2 family that produces a "
            "224-bit (28-byte) hash value. It is a truncated version of SHA-256, using the same "
            "algorithm but outputting a shorter digest. SHA-224 is used when a smaller hash size "
            "is preferred while maintaining strong security, commonly in digital signatures and "
            "certificate generation.",
    },
    {
        .name = "SHA-256 Hash",
        .file_key = "sha256",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = sha256_hash,
        .decode = NULL,
        .learn_text =
            "SHA-256 is part of the SHA-2 family of cryptographic hash functions and generates a "
            "256-bit (32-byte) hash value. It is currently considered secure and is widely used "
            "in blockchain, password hashing, digital signatures, and data integrity "
            "verification. SHA-256 offers strong resistance against collision and preimage "
            "attacks, making it a trusted standard today.",
    },
    {
        .name = "SHA-384 Hash",
        .file_key = "sha384",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = sha384_hash,
        .decode = NULL,
        .learn_text =
            "SHA-384 is a variant of the SHA-2 hash family that generates a 384-bit (48-byte) "
            "hash. It is a truncated version of SHA-512, optimized for 64-bit processors. "
            "SHA-384 provides a higher security margin than SHA-256 and SHA-224 due to its "
            "longer output and 64-bit internal operations, making it suitable for applications "
            "requiring robust collision resistance, such as secure communication protocols and "
            "cryptographic signatures.",
    },
    {
        .name = "SHA-512 Hash",
        .file_key = "sha512",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = sha512_hash,
        .decode = NULL,
        .learn_text =
            "SHA-512 (Secure Hash Algorithm 512) is a member of the SHA-2 family of cryptographic "
            "hash functions, developed by the NSA and standardized by NIST. It produces a 512-bit "
            "(64-byte) hash from any input, making it very resistant to collision and preimage "
            "attacks. SHA-512 operates on 1024-bit blocks and performs 80 rounds of complex "
            "mathematical operations involving bitwise logic, modular addition, and message "
            "expansion. It's widely used in digital signatures, certificates, and data integrity "
            "checks where strong cryptographic security is required. Although slower on 32-bit "
            "systems due to its large word size, SHA-512 is very efficient on 64-bit processors "
            "and remains a trusted standard in secure applications.",
    },
    {
        .name = "XXHash64",
        .file_key = "xx",
        .category = CipherCategoryHash,
        .key_kind = CipherKeyNone,
        .encode = xxhash_hash,
        .decode = NULL,
        .learn_text =
            "XXHash is a non-cryptographic hash function designed for high-performance hashing. "
            "Developed by Yann Collet, XXHash focuses on speed and efficiency, outperforming many "
            "traditional hash functions while maintaining a low collision rate. It comes in "
            "several versions, including XXH32, XXH64, and the newer XXH3, which offers improved "
            "performance and adaptability to modern CPUs. While XXHash is not suitable for "
            "cryptographic purposes due to its lack of security guarantees, it is widely used in "
            "performance-critical applications like databases, file systems, and compression "
            "tools.",
    },
    {
        .name = "Base16 Encoding",
        .file_key = "base16",
        .category = CipherCategoryEncoder,
        .key_kind = CipherKeyNone,
        .encode = base16_encode_wrap,
        .decode = base16_decode_wrap,
        .learn_text =
            "Base16, also known as hexadecimal encoding, is an encoding scheme that converts "
            "binary data into a set of 16 ASCII characters, using the digits 0-9 and letters "
            "A-F. It is commonly used for representing binary data in a human-readable format, "
            "such as displaying byte values, memory addresses, checksums, or color codes in "
            "web design. Each Base16 character represents exactly 4 bits of data, making it "
            "less space-efficient than Base32 or Base64 but simple to read and reason about, "
            "since each byte maps cleanly to exactly two hex characters. Unlike encryption or "
            "hashing, Base16 is not secure - it's simply a reversible way to encode data.",
    },
    {
        .name = "Base32 Encoding",
        .file_key = "base32",
        .category = CipherCategoryEncoder,
        .key_kind = CipherKeyNone,
        .encode = base32_encode_wrap,
        .decode = base32_decode_wrap,
        .learn_text =
            "Base32 is an encoding scheme that converts binary data into a set of 32 ASCII "
            "characters, using the characters A-Z and 2-7. It is commonly used for representing "
            "binary data in a human-readable and URL-safe format, especially when Base64 is not "
            "ideal due to case sensitivity or special characters. Each Base32 character "
            "represents 5 bits of data, making it slightly less space-efficient than Base64 but "
            "easier to handle in contexts like QR codes, file names, or secret keys (e.g., in "
            "two-factor authentication). Unlike encryption or hashing, Base32 is not secure - "
            "it's simply a reversible way to encode data.",
    },
    {
        .name = "Base58 Encoding",
        .file_key = "base58",
        .category = CipherCategoryEncoder,
        .key_kind = CipherKeyNone,
        .encode = base58_encode_wrap,
        .decode = base58_decode_wrap,
        .learn_text =
            "Base-58 is a binary-to-text encoding scheme used to represent large numbers or "
            "binary data in a more human-friendly format. It's most commonly used in "
            "cryptocurrencies like Bitcoin to encode addresses. Unlike Base-64, Base-58 removes "
            "easily confused characters such as zero, capital o, capital i, and lowercase L, to "
            "reduce human error when copying or typing. This makes Base-58 more suitable for "
            "user-facing strings while still being compact and efficient for encoding data.",
    },
    {
        .name = "Base64 Encoding",
        .file_key = "base64",
        .category = CipherCategoryEncoder,
        .key_kind = CipherKeyNone,
        .encode = base64_encode_wrap,
        .decode = base64_decode_wrap,
        .learn_text =
            "Base64 is a binary-to-text encoding scheme that represents binary data using 64 "
            "ASCII characters: A-Z, a-z, 0-9, +, and /. It works by dividing the input into 6-bit "
            "chunks and mapping each chunk to a character from the Base64 alphabet, often adding "
            "= as padding at the end to align the output. Base64 is commonly used to encode data "
            "for transmission over media that are designed to handle text, such as embedding "
            "images in HTML or safely transmitting binary data in email or JSON. Like Base-32 and "
            "Base-58, Base-64 is not secure as it is fully reversible.",
    },
};

const size_t kCipherCount = sizeof(kCiphers) / sizeof(kCiphers[0]);

const CipherDef* cipher_registry_find_by_name(const char* name) {
    for(size_t i = 0; i < kCipherCount; i++) {
        if(strcmp(kCiphers[i].name, name) == 0) return &kCiphers[i];
    }
    return NULL;
}

CipherResult cipher_registry_run(const CipherDef* def, bool is_decrypt, const char* input, int32_t key_a, int32_t key_b, const char* text_key) {
    furi_assert(def);

    if(def->key_kind == CipherKeyText && def->text_key_exact_len != 0) {
        size_t len = text_key ? strlen(text_key) : 0;
        if(len != def->text_key_exact_len) {
            char msg[48];
            snprintf(msg, sizeof(msg), "Key must be %u chars", def->text_key_exact_len);
            return err_result(msg);
        }
    }

    if(is_decrypt) {
        if(!def->decode) {
            return err_result("This entry has no decode");
        }
        return def->decode(input, key_a, key_b, text_key);
    }
    return def->encode(input, key_a, key_b, text_key);
}

void cipher_registry_build_filename(const CipherDef* def, bool is_decrypt, char* out, size_t out_size) {
    snprintf(out, out_size, "%s%s.txt", def->file_key, is_decrypt ? "_decrypt" : "");
}
