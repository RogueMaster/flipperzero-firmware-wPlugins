/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_LDS_H_
#define TINY_CRYPTO_LDS_H_
#include <tiny_crypto/der.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TC_LDS_MAX_GROUPS = 16 };
typedef struct {
  TC_bytes encoded, hashes, lds_version, unicode_version;
  TC_hash_algorithm hash;
  uint16_t groups;
  unsigned version;
} TC_LDS_security_object;

/* Read the DER LDSSecurityObject from CMS eContent (ICAO 9303 Part 10).
 * Supports versions 0 and 1, with 2..16 distinct data groups. Digest lengths
 * follow the declared SHA algorithm. Hash metadata is available independently
 * of compiled hash implementations. Version 1 requires LDS/Unicode versions.
 * All spans borrow encoded; groups has bit n-1 set for data group n.
 * Input, limits, frames, work and out are disjoint. Argument errors preserve
 * caller state; processing may consume work/scratch. out changes only on OK.
 * Authenticate the containing CMS and bind group numbers to application data
 * before relying on these hashes. Requires X509. */
TC_TLV_result TC_LDS_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_LDS_security_object* out);

/* Read CMS eContent from its complete BER OCTET STRING encoding, as returned
 * in TC_CMS_signed_data.content. A single content chunk is borrowed directly;
 * multiple chunks are joined in caller buffer storage. NULL/0 buffer storage
 * supports single-chunk input; insufficient capacity returns LIMIT. A buffer
 * as large as octets.length is sufficient. Returned spans borrow either the
 * input or buffer, which must remain stable while the object is used.
 * All inputs, metadata and writable ranges must be disjoint. Argument errors
 * preserve caller state; processing may change work, frames and buffer. out
 * changes only on OK. LDS schema always uses DER. Authenticate CMS separately.
 * Requires X509 and BER support. */
TC_TLV_result TC_LDS_read_content(TC_bytes octets, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work,
    uint8_t* buffer, size_t buffer_capacity, TC_LDS_security_object* out);

/* Find group 1..16 in a successfully parsed, unchanged object. Scans the
 * borrowed hash sequence; no index or digest copy is allocated. OK writes a
 * borrowed digest span; END means the group is absent. Other results are errors.
 * out changes only on OK. Keep object, its buffers and limits disjoint from
 * frames, work and out. Argument errors preserve caller state; processing may
 * consume work and scratch. CMS authentication remains a separate operation. */
TC_TLV_result TC_LDS_hash_find(const TC_LDS_security_object* object, unsigned number,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, TC_bytes* out);

/* Hash parts in order and compare the complete digest with a group's value.
 * Zero parts represent empty content. count is bounded by limits.max_elements;
 * total bytes are bounded by max_input/max_value. END means a missing group.
 * A disabled hash returns UNSUPPORTED. OK writes matched as 0 or 1; other
 * results preserve it. Work includes lookup, hashing and comparison.
 * Input parts may share bytes. All inputs and metadata must stay separate from
 * frames, work and matched. Preflight errors preserve caller state; processing
 * may consume work/scratch. The caller selects the exact bytes for its protocol
 * and authenticates the containing CMS before accepting a matching digest. */
TC_TLV_result TC_LDS_hash_check(const TC_LDS_security_object* object, unsigned number,
    const TC_bytes* parts, size_t count, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, int* matched);

#ifdef __cplusplus
}
#endif
#endif
