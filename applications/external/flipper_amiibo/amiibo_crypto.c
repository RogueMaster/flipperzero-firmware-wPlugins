#include "amiibo_crypto.h"

#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <stdlib.h>
#include <string.h>

#define AMIIBO_INTERNAL_SIZE 520
#define DRBG_OUTPUT          32

typedef struct {
    uint8_t aes_key[16];
    uint8_t aes_iv[16];
    uint8_t hmac_key[16];
} AmiiboDerived;

static bool hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t out[32]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    bool ok = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0 &&
              mbedtls_md_hmac_starts(&ctx, key, key_len) == 0 &&
              mbedtls_md_hmac_update(&ctx, data, data_len) == 0 &&
              mbedtls_md_hmac_finish(&ctx, out) == 0;
    mbedtls_md_free(&ctx);
    return ok;
}

static bool drbg_bytes(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* seed,
    size_t seed_len,
    uint8_t* out,
    size_t out_len) {
    mbedtls_md_context_t hmac;
    uint8_t buffer[2 + 96];
    uint8_t block[DRBG_OUTPUT];
    uint16_t iteration = 0;
    bool used = false;

    mbedtls_md_init(&hmac);
    bool ok = mbedtls_md_setup(&hmac, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0 &&
              mbedtls_md_hmac_starts(&hmac, key, key_len) == 0;
    if(!ok) goto cleanup;
    memcpy(buffer + 2, seed, seed_len);

    while(out_len > 0) {
        size_t take = out_len < DRBG_OUTPUT ? out_len : DRBG_OUTPUT;
        if(used && mbedtls_md_hmac_reset(&hmac) != 0) {
            ok = false;
            break;
        }
        used = true;
        buffer[0] = (uint8_t)(iteration >> 8);
        buffer[1] = (uint8_t)iteration;
        iteration++;
        if(mbedtls_md_hmac_update(&hmac, buffer, 2 + seed_len) != 0 ||
           mbedtls_md_hmac_finish(&hmac, block) != 0) {
            ok = false;
            break;
        }
        memcpy(out, block, take);
        out += take;
        out_len -= take;
    }

cleanup:
    mbedtls_md_free(&hmac);
    return ok;
}

static void master_from_bytes(AmiiboMasterKey* key, const uint8_t* raw) {
    memcpy(key->hmac_key, raw, 16);
    memcpy(key->type_string, raw + 16, 14);
    key->rfu = raw[30];
    key->magic_size = raw[31];
    memcpy(key->magic, raw + 32, 16);
    memcpy(key->xor_pad, raw + 48, 32);
}

bool amiibo_keys_from_bytes(AmiiboKeys* keys, const uint8_t* bytes, size_t size) {
    if(size != AMIIBO_KEY_SIZE) return false;
    master_from_bytes(&keys->data, bytes);
    master_from_bytes(&keys->tag, bytes + 80);
    if(keys->data.magic_size > 16 || keys->tag.magic_size > 16) return false;
    return true;
}

static size_t prepare_seed(const AmiiboMasterKey* key, const uint8_t seed[64], uint8_t* out) {
    uint8_t* cursor = out;
    size_t index = 0;
    for(; index < sizeof(key->type_string); index++) {
        cursor[index] = (uint8_t)key->type_string[index];
        if(key->type_string[index] == '\0') {
            cursor += index + 1;
            break;
        }
    }
    if(index == sizeof(key->type_string)) cursor += sizeof(key->type_string);

    size_t leading = 16U - key->magic_size;
    memcpy(cursor, seed, leading);
    cursor += leading;
    memcpy(cursor, key->magic, key->magic_size);
    cursor += key->magic_size;
    memcpy(cursor, seed + 0x10, 16);
    cursor += 16;
    for(size_t i = 0; i < 32; i++) {
        cursor[i] = (uint8_t)(seed[32 + i] ^ key->xor_pad[i]);
    }
    cursor += 32;
    return (size_t)(cursor - out);
}

static void calc_seed(const uint8_t* dump, uint8_t seed[64]) {
    memset(seed, 0, 64);
    memcpy(seed, dump + 0x029, 2);
    memcpy(seed + 0x10, dump + 0x1D4, 8);
    memcpy(seed + 0x18, dump + 0x1D4, 8);
    memcpy(seed + 0x20, dump + 0x1E8, 32);
}

static bool derive(const AmiiboMasterKey* master, const uint8_t* dump, AmiiboDerived* out) {
    uint8_t seed[64];
    uint8_t prepared[96];
    calc_seed(dump, seed);
    size_t prepared_len = prepare_seed(master, seed, prepared);
    return drbg_bytes(
        master->hmac_key,
        sizeof(master->hmac_key),
        prepared,
        prepared_len,
        (uint8_t*)out,
        sizeof(*out));
}

static void tag_to_internal(const uint8_t* tag, uint8_t* internal) {
    memset(internal, 0, AMIIBO_INTERNAL_SIZE);
    memcpy(internal + 0x000, tag + 0x008, 0x008);
    memcpy(internal + 0x008, tag + 0x080, 0x020);
    memcpy(internal + 0x028, tag + 0x010, 0x024);
    memcpy(internal + 0x04C, tag + 0x0A0, 0x168);
    memcpy(internal + 0x1B4, tag + 0x034, 0x020);
    memcpy(internal + 0x1D4, tag + 0x000, 0x008);
    memcpy(internal + 0x1DC, tag + 0x054, 0x02C);
}

static void internal_to_tag(const uint8_t* internal, uint8_t* tag) {
    memcpy(tag + 0x008, internal + 0x000, 0x008);
    memcpy(tag + 0x080, internal + 0x008, 0x020);
    memcpy(tag + 0x010, internal + 0x028, 0x024);
    memcpy(tag + 0x0A0, internal + 0x04C, 0x168);
    memcpy(tag + 0x034, internal + 0x1B4, 0x020);
    memcpy(tag + 0x000, internal + 0x1D4, 0x008);
    memcpy(tag + 0x054, internal + 0x1DC, 0x02C);
}

static bool amiibo_cipher(const AmiiboDerived* keys, const uint8_t* in, uint8_t* out) {
    mbedtls_aes_context aes;
    size_t offset = 0;
    uint8_t nonce[16];
    uint8_t stream[16];

    mbedtls_aes_init(&aes);
    bool ok = mbedtls_aes_setkey_enc(&aes, keys->aes_key, 128) == 0;
    memset(nonce, 0, sizeof(nonce));
    memset(stream, 0, sizeof(stream));
    memcpy(nonce, keys->aes_iv, sizeof(nonce));
    ok = ok &&
         mbedtls_aes_crypt_ctr(&aes, 0x188, &offset, nonce, stream, in + 0x02C, out + 0x02C) == 0;
    mbedtls_aes_free(&aes);
    if(!ok) return false;

    memcpy(out + 0x000, in + 0x000, 0x008);
    memcpy(out + 0x028, in + 0x028, 0x004);
    memcpy(out + 0x1D4, in + 0x1D4, 0x034);
    return true;
}

static bool amiibo_unpack(const AmiiboKeys* keys, const uint8_t* tag, uint8_t* plain) {
    uint8_t internal[AMIIBO_INTERNAL_SIZE];
    AmiiboDerived data_keys;
    AmiiboDerived tag_keys;

    tag_to_internal(tag, internal);
    if(!derive(&keys->data, internal, &data_keys) || !derive(&keys->tag, internal, &tag_keys) ||
       !amiibo_cipher(&data_keys, internal, plain) ||
       !hmac_sha256(
           tag_keys.hmac_key, sizeof(tag_keys.hmac_key), plain + 0x1D4, 0x34, plain + 0x1B4) ||
       !hmac_sha256(
           data_keys.hmac_key, sizeof(data_keys.hmac_key), plain + 0x029, 0x1DF, plain + 0x008)) {
        return false;
    }

    return memcmp(plain + 0x008, internal + 0x008, 32) == 0 &&
           memcmp(plain + 0x1B4, internal + 0x1B4, 32) == 0;
}

static bool amiibo_pack(const AmiiboKeys* keys, const uint8_t* plain, uint8_t* tag) {
    uint8_t cipher[AMIIBO_INTERNAL_SIZE];
    AmiiboDerived data_keys;
    AmiiboDerived tag_keys;
    mbedtls_md_context_t hmac;

    memset(cipher, 0, sizeof(cipher));
    if(!derive(&keys->tag, plain, &tag_keys) || !derive(&keys->data, plain, &data_keys) ||
       !hmac_sha256(
           tag_keys.hmac_key, sizeof(tag_keys.hmac_key), plain + 0x1D4, 0x34, cipher + 0x1B4)) {
        return false;
    }

    mbedtls_md_init(&hmac);
    bool ok = mbedtls_md_setup(&hmac, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0 &&
              mbedtls_md_hmac_starts(&hmac, data_keys.hmac_key, sizeof(data_keys.hmac_key)) == 0 &&
              mbedtls_md_hmac_update(&hmac, plain + 0x029, 0x18B) == 0 &&
              mbedtls_md_hmac_update(&hmac, cipher + 0x1B4, 0x20) == 0 &&
              mbedtls_md_hmac_update(&hmac, plain + 0x1D4, 0x34) == 0 &&
              mbedtls_md_hmac_finish(&hmac, cipher + 0x008) == 0;
    mbedtls_md_free(&hmac);
    if(!ok || !amiibo_cipher(&data_keys, plain, cipher)) return false;
    internal_to_tag(cipher, tag);
    return true;
}

void amiibo_uid_from_tag(const uint8_t tag[AMIIBO_TAG_SIZE], uint8_t uid[7]) {
    uid[0] = tag[0];
    uid[1] = tag[1];
    uid[2] = tag[2];
    uid[3] = tag[4];
    uid[4] = tag[5];
    uid[5] = tag[6];
    uid[6] = tag[7];
}

void amiibo_apply_password(uint8_t tag[AMIIBO_TAG_SIZE]) {
    uint8_t uid[7];
    amiibo_uid_from_tag(tag, uid);
    tag[532] = (uint8_t)(0xAA ^ uid[1] ^ uid[3]);
    tag[533] = (uint8_t)(0x55 ^ uid[2] ^ uid[4]);
    tag[534] = (uint8_t)(0xAA ^ uid[3] ^ uid[5]);
    tag[535] = (uint8_t)(0x55 ^ uid[4] ^ uid[6]);
    if(tag[536] == 0 && tag[537] == 0) {
        tag[536] = 0x80;
        tag[537] = 0x80;
    }
}

void amiibo_fill_blank_trailer(uint8_t tag[AMIIBO_TAG_SIZE]) {
    bool blank = true;
    for(size_t i = 524; i < 532; i++) {
        if(tag[i] != 0) {
            blank = false;
            break;
        }
    }
    if(!blank) return;
    tag[520] = 0x01;
    tag[521] = 0x00;
    tag[522] = 0x0F;
    tag[523] = 0x00;
    tag[524] = 0x00;
    tag[525] = 0x00;
    tag[526] = 0x00;
    tag[527] = 0x04;
    tag[528] = 0x5F;
    tag[529] = 0x00;
    tag[530] = 0x00;
    tag[531] = 0x00;
}

AmiiboResult
    amiibo_set_uid(uint8_t tag[AMIIBO_TAG_SIZE], const AmiiboKeys* keys, const uint8_t uid[7]) {
    uint8_t* plain = malloc(AMIIBO_INTERNAL_SIZE);
    uint8_t* rewritten = malloc(AMIIBO_TAG_SIZE);
    AmiiboResult result = AmiiboBadSignature;

    if(plain == NULL || rewritten == NULL) {
        free(plain);
        free(rewritten);
        return AmiiboBadKeys;
    }

    memcpy(rewritten, tag, AMIIBO_TAG_SIZE);
    if(amiibo_unpack(keys, tag, plain)) {
        plain[0x1D4 + 0] = uid[0];
        plain[0x1D4 + 1] = uid[1];
        plain[0x1D4 + 2] = uid[2];
        plain[0x1D4 + 3] = (uint8_t)(0x88 ^ uid[0] ^ uid[1] ^ uid[2]);
        plain[0x1D4 + 4] = uid[3];
        plain[0x1D4 + 5] = uid[4];
        plain[0x1D4 + 6] = uid[5];
        plain[0x1D4 + 7] = uid[6];
        plain[0] = (uint8_t)(uid[3] ^ uid[4] ^ uid[5] ^ uid[6]);
        if(amiibo_pack(keys, plain, rewritten)) {
            amiibo_apply_password(rewritten);
            memcpy(tag, rewritten, AMIIBO_TAG_SIZE);
            result = AmiiboOk;
        } else {
            result = AmiiboBadKeys;
        }
    }

    free(plain);
    free(rewritten);
    return result;
}
