// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter.h
 * @brief Pure domain logic for Xiaomi air purifier filter tags.
 *
 * Xiaomi air purifier filters carry an NTAG213 (NfcA / ISO14443-3A) tag that the
 * appliance uses to enforce a "filter life" DRM. The relevant facts, all verified
 * against real tag dumps (see docs/protocol.md):
 *
 *   - The 4-byte NTAG password is derived deterministically from the 7-byte tag
 *     UID: it is a fixed selection of bytes from SHA-1(UID).
 *   - The usage counter lives in page 8, stored as a little-endian uint32. Writing
 *     0x00000000 there resets the filter to 100% life.
 *   - Pages 4..8 are password-protected (READ and WRITE), so authentication is
 *     required before the counter can be read or cleared. The UID (pages 0-1) is
 *     always readable.
 *
 * This translation unit has NO firmware dependency, which makes it fully unit
 * testable on the host. The very same object code is linked into the Flipper app.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Length of an NTAG213 (double-size) UID, in bytes. */
#define XIAOMI_FILTER_UID_LEN (7U)

/** @brief NTAG password length, in bytes. */
#define XIAOMI_FILTER_PWD_LEN (4U)

/** @brief NTAG page size, in bytes. */
#define XIAOMI_FILTER_PAGE_SIZE (4U)

/** @brief Page holding the little-endian usage counter. Zeroing it resets to 100%. */
#define XIAOMI_FILTER_COUNTER_PAGE (8U)

/** @brief First page protected by the password (AUTH0). */
#define XIAOMI_FILTER_AUTH0_PAGE (4U)

/** @brief Page holding the RFID factory id (first half of the product code). */
#define XIAOMI_FILTER_FACTORY_ID_PAGE (4U)

/** @brief Page holding the RFID product id (second half of the product code). */
#define XIAOMI_FILTER_PRODUCT_ID_PAGE (5U)

/** @brief Size of the printable product code string, including the NUL terminator. */
#define XIAOMI_FILTER_PRODUCT_CODE_SIZE (5U)

/**
 * @brief Derive the NTAG password from a filter tag UID.
 *
 * Reproduces the vendor algorithm:
 *   h = SHA1(uid)
 *   pwd = { h[h[0] % 20], h[(h[0]+5) % 20], h[(h[0]+13) % 20], h[(h[0]+17) % 20] }
 *
 * @param[in]  uid     pointer to the raw UID bytes
 * @param[in]  uid_len length of @p uid; must be XIAOMI_FILTER_UID_LEN
 * @param[out] out_pwd destination for the XIAOMI_FILTER_PWD_LEN password bytes
 * @return true on success, false if @p uid_len is not XIAOMI_FILTER_UID_LEN
 */
bool xiaomi_filter_derive_password(
    const uint8_t* uid,
    size_t uid_len,
    uint8_t out_pwd[XIAOMI_FILTER_PWD_LEN]);

/**
 * @brief Assemble the 7-byte UID from NTAG memory pages 0 and 1.
 *
 * Layout: page0 = { UID0, UID1, UID2, BCC0 }, page1 = { UID3, UID4, UID5, UID6 }.
 *
 * @param[in]  page0   4 bytes of page 0
 * @param[in]  page1   4 bytes of page 1
 * @param[out] out_uid destination for XIAOMI_FILTER_UID_LEN bytes
 */
void xiaomi_filter_uid_from_pages(
    const uint8_t page0[XIAOMI_FILTER_PAGE_SIZE],
    const uint8_t page1[XIAOMI_FILTER_PAGE_SIZE],
    uint8_t out_uid[XIAOMI_FILTER_UID_LEN]);

/**
 * @brief Decode the usage counter stored in a page as a little-endian uint32.
 *
 * @param[in] page 4 bytes of the counter page
 * @return the decoded counter value (arbitrary vendor time units)
 */
uint32_t xiaomi_filter_counter_from_page(const uint8_t page[XIAOMI_FILTER_PAGE_SIZE]);

/**
 * @brief Test whether a page is all zeroes (used to verify a successful reset).
 *
 * @param[in] page 4 bytes to test
 * @return true if every byte is zero
 */
bool xiaomi_filter_page_is_zero(const uint8_t page[XIAOMI_FILTER_PAGE_SIZE]);

/**
 * @brief Extract the printable product code (e.g. "AP11", "JDA0").
 *
 * The code is the concatenation of the two meaningful ASCII bytes of the factory
 * id page and the product id page. Non-printable bytes are rendered as '?'.
 *
 * @param[in]  factory_id_page 4 bytes of the factory id page (page 4)
 * @param[in]  product_id_page 4 bytes of the product id page (page 5)
 * @param[out] out_code        NUL-terminated buffer of XIAOMI_FILTER_PRODUCT_CODE_SIZE bytes
 */
void xiaomi_filter_product_code(
    const uint8_t factory_id_page[XIAOMI_FILTER_PAGE_SIZE],
    const uint8_t product_id_page[XIAOMI_FILTER_PAGE_SIZE],
    char out_code[XIAOMI_FILTER_PRODUCT_CODE_SIZE]);

#ifdef __cplusplus
}
#endif
