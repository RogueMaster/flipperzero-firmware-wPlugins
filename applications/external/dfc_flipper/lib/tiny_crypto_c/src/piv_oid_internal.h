/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PIV_OID_INTERNAL_H_
#define TC_PIV_OID_INTERNAL_H_
#include <tiny_crypto/piv_oid.h>

/* Registration arc of an identifier: the PIV root 2.16.840.1.101.3 or the
 * TWIC root 1.3.6.1.4.1.29138 (TWIC Part 2 v5 section 6). */
typedef enum { TC_PIV_OID_NAMESPACE_PIV, TC_PIV_OID_NAMESPACE_TWIC } tc_piv_oid_namespace;

/* Borrow the encoded OID contents for id in one namespace, excluding the
 * ASN.1 tag and length. The span has static storage, so callers may keep the
 * pointer, for example as a one-element initial policy set. Returns NULL for
 * UNKNOWN, an unlisted id, an invalid namespace, or a namespace that section 6
 * gives no identifier for id. */
const TC_bytes* tc_piv_oid_contents(TC_PIV_oid id, tc_piv_oid_namespace space);

#endif
