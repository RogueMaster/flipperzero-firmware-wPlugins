// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter.c
 * @brief Pure domain logic for Xiaomi air purifier filter tags.
 */
#include "xiaomi_filter.h"

#include "sha1.h"

bool xiaomi_filter_derive_password(
    const uint8_t* uid,
    size_t uid_len,
    uint8_t out_pwd[XIAOMI_FILTER_PWD_LEN]) {
    if(uid == NULL || out_pwd == NULL || uid_len != XIAOMI_FILTER_UID_LEN) {
        return false;
    }

    uint8_t h[SHA1_DIGEST_SIZE];
    sha1(uid, uid_len, h);

    // The vendor picks four bytes out of the first 20 (SHA-1 digest length),
    // indexed relative to the value of the first digest byte.
    const uint8_t base = h[0] % SHA1_DIGEST_SIZE;
    static const uint8_t offsets[XIAOMI_FILTER_PWD_LEN] = {0, 5, 13, 17};

    for(size_t i = 0; i < XIAOMI_FILTER_PWD_LEN; i++) {
        out_pwd[i] = h[(base + offsets[i]) % SHA1_DIGEST_SIZE];
    }

    return true;
}

void xiaomi_filter_uid_from_pages(
    const uint8_t page0[XIAOMI_FILTER_PAGE_SIZE],
    const uint8_t page1[XIAOMI_FILTER_PAGE_SIZE],
    uint8_t out_uid[XIAOMI_FILTER_UID_LEN]) {
    // Page 0 carries UID bytes 0..2 (byte 3 is the BCC0 check byte);
    // page 1 carries UID bytes 3..6.
    out_uid[0] = page0[0];
    out_uid[1] = page0[1];
    out_uid[2] = page0[2];
    out_uid[3] = page1[0];
    out_uid[4] = page1[1];
    out_uid[5] = page1[2];
    out_uid[6] = page1[3];
}

uint32_t xiaomi_filter_counter_from_page(const uint8_t page[XIAOMI_FILTER_PAGE_SIZE]) {
    return (uint32_t)page[0] | ((uint32_t)page[1] << 8) | ((uint32_t)page[2] << 16) |
           ((uint32_t)page[3] << 24);
}

bool xiaomi_filter_page_is_zero(const uint8_t page[XIAOMI_FILTER_PAGE_SIZE]) {
    for(size_t i = 0; i < XIAOMI_FILTER_PAGE_SIZE; i++) {
        if(page[i] != 0) {
            return false;
        }
    }
    return true;
}

static char xiaomi_filter_printable(uint8_t byte) {
    return (byte >= 0x20 && byte < 0x7F) ? (char)byte : '?';
}

void xiaomi_filter_product_code(
    const uint8_t factory_id_page[XIAOMI_FILTER_PAGE_SIZE],
    const uint8_t product_id_page[XIAOMI_FILTER_PAGE_SIZE],
    char out_code[XIAOMI_FILTER_PRODUCT_CODE_SIZE]) {
    out_code[0] = xiaomi_filter_printable(factory_id_page[2]);
    out_code[1] = xiaomi_filter_printable(factory_id_page[3]);
    out_code[2] = xiaomi_filter_printable(product_id_page[2]);
    out_code[3] = xiaomi_filter_printable(product_id_page[3]);
    out_code[4] = '\0';
}
