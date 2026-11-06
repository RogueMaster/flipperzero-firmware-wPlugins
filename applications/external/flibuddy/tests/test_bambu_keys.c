#include "minitest.h"
#include "vectors/bambu_keys.h"

#include "bambu_keys.h"
#include "crypto/hkdf_sha256.h"
#include "crypto/hmac_sha256.h"
#include "crypto/sha256.h"

static void test_sha256_known_answers(void) {
    for(size_t i = 0; i < SHA256_VECTOR_COUNT; i++) {
        const Sha256Vector* v = &SHA256_VECTORS[i];
        uint8_t digest[FB_SHA256_DIGEST_LEN];
        fb_sha256(v->message, v->message_len, digest);
        if(memcmp(digest, v->digest, sizeof(digest)) != 0) printf("  sha256 %s\n", v->name);
        CHECK_MEM_EQ(digest, v->digest, sizeof(digest));
    }
}

static void test_sha256_streaming_matches_one_shot(void) {
    /* Feed every vector in uneven chunks so buffered partial blocks are hit. */
    static const size_t chunks[] = {1, 3, 7, 13, 64, 65};
    for(size_t i = 0; i < SHA256_VECTOR_COUNT; i++) {
        const Sha256Vector* v = &SHA256_VECTORS[i];
        for(size_t c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++) {
            FbSha256 ctx;
            uint8_t digest[FB_SHA256_DIGEST_LEN];
            fb_sha256_init(&ctx);
            for(size_t off = 0; off < v->message_len; off += chunks[c]) {
                size_t n = v->message_len - off < chunks[c] ? v->message_len - off : chunks[c];
                fb_sha256_update(&ctx, (const uint8_t*)v->message + off, n);
            }
            fb_sha256_final(&ctx, digest);
            CHECK_MEM_EQ(digest, v->digest, sizeof(digest));
        }
    }
}

static void test_sha256_million_a(void) {
    uint8_t chunk[1000];
    memset(chunk, 'a', sizeof(chunk));
    FbSha256 ctx;
    fb_sha256_init(&ctx);
    for(int i = 0; i < 1000; i++)
        fb_sha256_update(&ctx, chunk, sizeof(chunk));
    uint8_t digest[FB_SHA256_DIGEST_LEN];
    fb_sha256_final(&ctx, digest);
    CHECK_MEM_EQ(digest, SHA256_MILLION_A, sizeof(digest));
}

static void test_hmac_rfc4231(void) {
    for(size_t i = 0; i < HMAC_VECTOR_COUNT; i++) {
        const HmacVector* v = &HMAC_VECTORS[i];
        uint8_t mac[FB_SHA256_DIGEST_LEN];
        fb_hmac_sha256(
            (const uint8_t*)v->key, v->key_len, (const uint8_t*)v->data, v->data_len, mac);
        if(memcmp(mac, v->mac, v->mac_len) != 0) printf("  hmac %s\n", v->name);
        CHECK_MEM_EQ(mac, v->mac, v->mac_len);
    }
}

static void test_hmac_streaming_matches_one_shot(void) {
    const HmacVector* v = &HMAC_VECTORS[6]; /* long key and long data */
    FbHmacSha256 ctx;
    fb_hmac_sha256_init(&ctx, (const uint8_t*)v->key, v->key_len);
    for(size_t off = 0; off < v->data_len; off += 10) {
        size_t n = v->data_len - off < 10 ? v->data_len - off : 10;
        fb_hmac_sha256_update(&ctx, (const uint8_t*)v->data + off, n);
    }
    uint8_t mac[FB_SHA256_DIGEST_LEN];
    fb_hmac_sha256_final(&ctx, mac);
    CHECK_MEM_EQ(mac, v->mac, v->mac_len);
}

static void test_hkdf_rfc5869(void) {
    for(size_t i = 0; i < HKDF_VECTOR_COUNT; i++) {
        const HkdfVector* v = &HKDF_VECTORS[i];
        uint8_t okm[82];
        memset(okm, 0xEE, sizeof(okm));
        bool ok = fb_hkdf_sha256(
            (const uint8_t*)v->salt,
            v->salt_len,
            (const uint8_t*)v->ikm,
            v->ikm_len,
            (const uint8_t*)v->info,
            v->info_len,
            okm,
            v->okm_len);
        CHECK(ok);
        if(memcmp(okm, v->okm, v->okm_len) != 0) printf("  hkdf %s\n", v->name);
        CHECK_MEM_EQ(okm, v->okm, v->okm_len);
        /* nothing written past the requested length */
        for(size_t b = v->okm_len; b < sizeof(okm); b++)
            CHECK_EQ_INT(okm[b], 0xEE);
    }
}

static void test_hkdf_rejects_bad_lengths(void) {
    uint8_t out[1] = {0};
    const uint8_t ikm[1] = {0x42};
    CHECK(!fb_hkdf_sha256(NULL, 0, ikm, sizeof(ikm), NULL, 0, out, 0));
    CHECK(!fb_hkdf_sha256(NULL, 0, ikm, sizeof(ikm), NULL, 0, out, 255 * 32 + 1));
    CHECK_EQ_INT(out[0], 0);
}

static void test_bambu_derive_keys_matches_vectors(void) {
    for(size_t i = 0; i < BAMBU_UID_VECTOR_COUNT; i++) {
        const BambuUidVector* v = &BAMBU_UID_VECTORS[i];
        uint8_t keys[BAMBU_KEYS_LEN];
        bambu_derive_keys(v->uid, keys);
        CHECK_MEM_EQ(keys, v->keys, sizeof(keys));
    }
}

static void test_bambu_sector_key(void) {
    const BambuUidVector* v = &BAMBU_UID_VECTORS[0];
    uint8_t keys[BAMBU_KEYS_LEN];
    bambu_derive_keys(v->uid, keys);
    for(uint8_t sector = 0; sector < BAMBU_SECTOR_COUNT; sector++) {
        uint8_t key[BAMBU_KEY_LEN];
        CHECK(bambu_sector_key(keys, sector, key));
        CHECK_MEM_EQ(key, v->keys + sector * BAMBU_KEY_LEN, BAMBU_KEY_LEN);
    }
}

static void test_bambu_sector_key_out_of_range(void) {
    uint8_t keys[BAMBU_KEYS_LEN] = {0};
    uint8_t key[BAMBU_KEY_LEN] = {1, 2, 3, 4, 5, 6};
    const uint8_t untouched[BAMBU_KEY_LEN] = {1, 2, 3, 4, 5, 6};
    CHECK(!bambu_sector_key(keys, BAMBU_SECTOR_COUNT, key));
    CHECK(!bambu_sector_key(keys, 255, key));
    CHECK_MEM_EQ(key, untouched, BAMBU_KEY_LEN);
}

int main(void) {
    RUN_TEST(test_sha256_known_answers);
    RUN_TEST(test_sha256_streaming_matches_one_shot);
    RUN_TEST(test_sha256_million_a);
    RUN_TEST(test_hmac_rfc4231);
    RUN_TEST(test_hmac_streaming_matches_one_shot);
    RUN_TEST(test_hkdf_rfc5869);
    RUN_TEST(test_hkdf_rejects_bad_lengths);
    RUN_TEST(test_bambu_derive_keys_matches_vectors);
    RUN_TEST(test_bambu_sector_key);
    RUN_TEST(test_bambu_sector_key_out_of_range);
    return MINITEST_RESULT();
}
