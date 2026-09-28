/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_SECURITY_H
#define TINY_CRYPTO_PIV_SECURITY_H
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_SECURITY_CONTENTS,
  TC_PIV_SECURITY_CONTAINER
} TC_PIV_security_encoding;

typedef struct {
  TC_bytes mapping, cms;
  uint16_t groups;
} TC_PIV_security_object;

/* Read the BA/BB/FE fields of a PIV or TWIC security object. CONTAINER includes
 * the outer 53 TLV; CONTENTS starts at BA. The mapping contains three-byte
 * records: group number followed by a big-endian container ID. Group numbers
 * and container IDs must be unique. cms and mapping borrow the unchanged input.
 * Input and out must be disjoint. Only OK writes out. This checks the container
 * schema; authenticate CMS and reconcile its LDS groups before using the map.
 * Requires X509. */
TC_TLV_result TC_PIV_security_read(TC_bytes encoded,
    TC_PIV_security_encoding encoding, TC_PIV_security_object* out);

/* Find a container ID in a successfully parsed, unchanged object. Scans at
 * most 16 mapping records and writes its group number only on OK. END means
 * absent. Keep number disjoint from object and its borrowed spans. Compare
 * object.groups with the authenticated LDS groups before checking hashes. */
TC_TLV_result TC_PIV_security_group_find(const TC_PIV_security_object* object,
    uint16_t container, unsigned* number);

#ifdef __cplusplus
}
#endif
#endif
