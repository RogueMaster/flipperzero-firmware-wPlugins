// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file test_xiaomi_filter.c
 * @brief Host unit tests for the pure filter core (no firmware dependency).
 *
 * Builds and runs on any host toolchain. Exercises the exact object code that is
 * linked into the Flipper app: src/core/sha1.c and src/core/xiaomi_filter.c.
 */
#include "../src/core/sha1.h"
#include "../src/core/xiaomi_filter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, ...)                               \
    do {                                               \
        g_checks++;                                    \
        if(!(cond)) {                                  \
            g_failures++;                              \
            printf("  FAIL: ");                        \
            printf(__VA_ARGS__);                       \
            printf("  (%s:%d)\n", __FILE__, __LINE__); \
        }                                              \
    } while(0)

/** @brief Parse an even-length hex string into bytes. Returns byte count. */
static size_t hex_to_bytes(const char* hex, uint8_t* out, size_t out_cap) {
    size_t len = strlen(hex);
    size_t n = len / 2;
    if(n > out_cap) n = out_cap;
    for(size_t i = 0; i < n; i++) {
        unsigned int byte = 0;
        sscanf(hex + i * 2, "%2x", &byte);
        out[i] = (uint8_t)byte;
    }
    return n;
}

static void bytes_to_hex(const uint8_t* bytes, size_t n, char* out) {
    for(size_t i = 0; i < n; i++) {
        sprintf(out + i * 2, "%02X", bytes[i]);
    }
    out[n * 2] = '\0';
}

/* ------------------------------------------------------------------------- */

static void test_sha1_known_vector(void) {
    printf("test_sha1_known_vector\n");
    // FIPS 180-1 example: SHA1("abc").
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1((const uint8_t*)"abc", 3, digest);
    char hex[SHA1_DIGEST_SIZE * 2 + 1];
    bytes_to_hex(digest, SHA1_DIGEST_SIZE, hex);
    CHECK(strcmp(hex, "A9993E364706816ABA3E25717850C26C9CD0D89D") == 0, "SHA1(\"abc\") = %s", hex);
}

static void test_password_golden_vectors(void) {
    printf("test_password_golden_vectors\n");
    // Ground-truth vectors:
    //   [1..2] from the vendor's own assert() lines (flamingo-tech RE writeup)
    //   [3..5] read back from the PWD page (page 43) of real filter tag dumps
    struct {
        const char* uid;
        const char* pwd;
        const char* note;
    } vectors[] = {
        {"04A03CAA1E7080", "CD91AFCC", "README assert / tag dump f3"},
        {"04112233445566", "EC9805C8", "README assert"},
        {"0404D972BA6C81", "103AF4A2", "tag dump f1"},
        {"047FD6E2EA6C80", "7D295EE7", "tag dump f2"},
        {"04FA3CAA1E7080", "95BF2720", "tag dump f4"},
    };

    for(size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        uint8_t uid[XIAOMI_FILTER_UID_LEN];
        size_t uid_len = hex_to_bytes(vectors[i].uid, uid, sizeof(uid));

        uint8_t pwd[XIAOMI_FILTER_PWD_LEN];
        bool ok = xiaomi_filter_derive_password(uid, uid_len, pwd);
        CHECK(ok, "derive returned false for UID %s", vectors[i].uid);

        char hex[XIAOMI_FILTER_PWD_LEN * 2 + 1];
        bytes_to_hex(pwd, XIAOMI_FILTER_PWD_LEN, hex);
        CHECK(
            strcmp(hex, vectors[i].pwd) == 0,
            "UID %s -> %s, expected %s (%s)",
            vectors[i].uid,
            hex,
            vectors[i].pwd,
            vectors[i].note);
    }
}

static void test_password_rejects_bad_length(void) {
    printf("test_password_rejects_bad_length\n");
    uint8_t uid[4] = {0x04, 0x00, 0x00, 0x00};
    uint8_t pwd[XIAOMI_FILTER_PWD_LEN];
    CHECK(!xiaomi_filter_derive_password(uid, 4, pwd), "4-byte UID must be rejected");
    CHECK(!xiaomi_filter_derive_password(NULL, 7, pwd), "NULL UID must be rejected");
    CHECK(
        !xiaomi_filter_derive_password(uid, XIAOMI_FILTER_UID_LEN, NULL),
        "NULL output must be rejected");
}

static void test_uid_from_pages(void) {
    printf("test_uid_from_pages\n");
    // Real tag f1: page0 = 04 04 D9 51, page1 = 72 BA 6C 81 -> UID 0404D972BA6C81.
    uint8_t page0[4] = {0x04, 0x04, 0xD9, 0x51};
    uint8_t page1[4] = {0x72, 0xBA, 0x6C, 0x81};
    uint8_t uid[XIAOMI_FILTER_UID_LEN];
    xiaomi_filter_uid_from_pages(page0, page1, uid);
    uint8_t expected[XIAOMI_FILTER_UID_LEN] = {0x04, 0x04, 0xD9, 0x72, 0xBA, 0x6C, 0x81};
    CHECK(memcmp(uid, expected, XIAOMI_FILTER_UID_LEN) == 0, "UID assembly mismatch");
}

static void test_counter_from_page(void) {
    printf("test_counter_from_page\n");
    // Real dumps of the same filter: 98% state and 99% state (little-endian).
    uint8_t page_98[4] = {0xA4, 0x29, 0x02, 0x00};
    uint8_t page_99[4] = {0xDA, 0x67, 0x01, 0x00};
    uint8_t page_fresh[4] = {0x00, 0x00, 0x00, 0x00};
    CHECK(xiaomi_filter_counter_from_page(page_98) == 141732u, "98%% counter");
    CHECK(xiaomi_filter_counter_from_page(page_99) == 92122u, "99%% counter");
    CHECK(xiaomi_filter_counter_from_page(page_fresh) == 0u, "fresh counter");
    // Lower counter means more life left.
    CHECK(
        xiaomi_filter_counter_from_page(page_99) < xiaomi_filter_counter_from_page(page_98),
        "counter must be monotonic with usage");
}

static void test_page_is_zero(void) {
    printf("test_page_is_zero\n");
    uint8_t zero[4] = {0, 0, 0, 0};
    uint8_t nonzero[4] = {0, 0, 1, 0};
    CHECK(xiaomi_filter_page_is_zero(zero), "all-zero page");
    CHECK(!xiaomi_filter_page_is_zero(nonzero), "non-zero page");
}

static void test_product_code(void) {
    printf("test_product_code\n");
    char code[XIAOMI_FILTER_PRODUCT_CODE_SIZE];
    // Filters 1/2: factory 00 00 41 50, product 00 00 31 31 -> "AP11".
    uint8_t f4a[4] = {0x00, 0x00, 0x41, 0x50};
    uint8_t f5a[4] = {0x00, 0x00, 0x31, 0x31};
    xiaomi_filter_product_code(f4a, f5a, code);
    CHECK(strcmp(code, "AP11") == 0, "product code AP11, got %s", code);
    // Filters 3/4: factory 00 00 4A 44, product 00 00 41 30 -> "JDA0".
    uint8_t f4b[4] = {0x00, 0x00, 0x4A, 0x44};
    uint8_t f5b[4] = {0x00, 0x00, 0x41, 0x30};
    xiaomi_filter_product_code(f4b, f5b, code);
    CHECK(strcmp(code, "JDA0") == 0, "product code JDA0, got %s", code);
    // Non-printable bytes render as '?'.
    uint8_t f4c[4] = {0x00, 0x00, 0x00, 0xFF};
    uint8_t f5c[4] = {0x00, 0x00, 0x41, 0x42};
    xiaomi_filter_product_code(f4c, f5c, code);
    CHECK(strcmp(code, "??AB") == 0, "non-printable rendering, got %s", code);
}

int main(void) {
    printf("Running xiaomi_filter host tests\n\n");

    test_sha1_known_vector();
    test_password_golden_vectors();
    test_password_rejects_bad_length();
    test_uid_from_pages();
    test_counter_from_page();
    test_page_is_zero();
    test_product_code();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
