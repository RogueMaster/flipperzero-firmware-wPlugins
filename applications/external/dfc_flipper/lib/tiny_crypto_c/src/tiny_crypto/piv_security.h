/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC security object container reader: data-group mapping and the
 * CMS value.
 * Standards: SP 800-73-5 Part 1.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: schema checks only. Authenticate the CMS before using the map.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_SECURITY_H
#define TINY_CRYPTO_PIV_SECURITY_H
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_SECURITY_CONTENTS, TC_PIV_SECURITY_CONTAINER } TC_PIV_security_encoding;

typedef struct {
  TC_bytes mapping, cms;
  uint16_t groups;
} TC_PIV_security_object;

#if TC_ENABLE_PIV_OBJECTS
/* Read the BA, BB and FE fields of a PIV or TWIC Security Object
 * (SP 800-73-5 Part 1 section 3.1.7). CONTAINER includes the outer 53 TLV.
 * CONTENTS starts at BA. The mapping holds 1 to TC_LDS_MAX_GROUPS three-byte
 * records: a group number 1..16 followed by a big-endian container ID. Group
 * numbers and container IDs are unique. groups has bit n-1 set for each
 * group n. The CMS value is nonempty and FE is empty. cms and mapping borrow
 * the unchanged input. encoded and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, an unknown encoding or overlap. MORE when input ends inside the
 * outer 53 object or a field of CONTENTS. INVALID for a missing, reordered
 * or extra field, trailing bytes or a bad mapping. out changes only on OK.
 * Authenticate the CMS and reconcile its LDS groups before using the map. */
TC_TLV_result TC_PIV_security_read(TC_bytes encoded, TC_PIV_security_encoding encoding,
                                   TC_PIV_security_object* out);

/* Find the group number of container in an object from
 * TC_PIV_security_read. Scans at most 16 mapping records. number must be
 * disjoint from object and its borrowed spans. Charges no work.
 * Returns OK with number written. END when the container is absent.
 * ARGUMENT for NULL arguments or overlap. INVALID for a mapping that fails
 * the read checks or no longer matches object->groups. number changes only
 * on OK. Compare object->groups with the authenticated LDS groups before
 * checking hashes. */
TC_TLV_result TC_PIV_security_group_find(const TC_PIV_security_object* object, uint16_t container,
                                         unsigned* number);
#endif

#ifdef __cplusplus
}
#endif
#endif
