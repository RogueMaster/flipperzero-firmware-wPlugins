/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* LDS security object reader and data-group hash checks for PIV and TWIC
 * security objects.
 * Standards: ICAO Doc 9303 Part 10, SP 800-73-5 Part 1.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: the reader parses content. CMS authentication is separate.
 * Contracts: docs/api.md, including its size_t work units. Guide: docs/lds.md. */
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

#if TC_ENABLE_PIV_OBJECTS
/* Read the DER LDSSecurityObject from CMS eContent (ICAO Doc 9303 Part 10,
 * SP 800-73-5 Part 1 section 3.1.7). Supports versions 0 and 1 with 2..16
 * distinct data groups numbered 1..16. Digest lengths follow the declared
 * SHA-1 or SHA-2 algorithm in every build configuration. Version 1
 * requires the 4-digit LDS and 6-digit Unicode version strings. groups has
 * bit n-1 set for data group n. All spans borrow encoded. encoded, limits,
 * frames, work and out must be disjoint.
 *
 * Work: a 10-unit storage check and the bytes of each parsed field.
 * Returns OK with out written. ARGUMENT for NULL limits, work or out, or
 * overlap, with all state unchanged. LIMIT for exhausted limits, frames or
 * work. UNSUPPORTED for a version above 1 or a digest outside SHA-1 and
 * SHA-2. INVALID for schema failures, a digest of the wrong length, a
 * repeated or out-of-range group, or fewer than two groups. out changes only
 * on OK. Frames and work are provisional on failure. Authenticate the
 * containing CMS and bind group numbers to application data before relying
 * on these hashes. */
TC_TLV_result TC_LDS_read(TC_bytes encoded, const TC_TLV_limits* limits, TC_TLV_frames frames,
                          size_t* work, TC_LDS_security_object* out);

/* Read an LDSSecurityObject from CMS eContent given as its complete BER OCTET
 * STRING encoding, as in TC_CMS_signed_data.content. A single content chunk is
 * borrowed directly. Multiple chunks are joined in buffer. A buffer of
 * octets.length bytes is always sufficient, and NULL/0 serves single-chunk
 * input. Returned spans borrow the input or buffer, which must stay stable
 * while the object is used. octets, limits, frames, work, buffer and out must
 * be disjoint. The LDS schema uses DER.
 *
 * Work: one unit per storage comparison, the OCTET STRING framing and copied
 * bytes, then TC_LDS_read.
 * Returns OK with out written. ARGUMENT for NULL limits, work or out, or
 * overlap, with all state unchanged. LIMIT for a buffer too small for the
 * joined content, or exhausted limits, frames or work. Other statuses follow
 * TC_LDS_read. out changes only on OK. buffer, frames and work are
 * provisional on failure. Authenticate the CMS separately. */
TC_TLV_result TC_LDS_read_content(TC_bytes octets, const TC_TLV_limits* limits,
                                  TC_TLV_frames frames, size_t* work, TC_buffer buffer,
                                  TC_LDS_security_object* out);

/* Find the digest of group number 1..16 in an object from TC_LDS_read. Scans
 * the borrowed hash sequence in place and checks that the group set still
 * equals object->groups. object, its buffers and limits must be disjoint from
 * frames, work and out.
 *
 * Work: 20 units for the storage checks and the bytes of the hash sequence.
 * Returns OK with a borrowed digest span in out. END when the group is
 * absent. ARGUMENT for NULL arguments, a number outside 1..16 or overlap,
 * with all state unchanged. UNSUPPORTED for an object hash outside SHA-1 and
 * SHA-2. LIMIT for exhausted limits, frames or work. INVALID for a hash
 * sequence that fails the read checks or no longer matches object->groups.
 * out changes only on OK. */
TC_TLV_result TC_LDS_hash_find(const TC_LDS_security_object* object, unsigned number,
                               const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                               TC_bytes* out);

/* Hash parts in order with the object's algorithm and compare the complete
 * digest with the value of group number in constant time. Zero parts
 * represent empty content. The caller selects the exact bytes for its
 * protocol and authenticates the containing CMS before accepting a match.
 * - count is at most limits->max_elements. Total bytes are bounded by
 *   max_input and max_value.
 * - Parts may share bytes. object, its buffers, limits, parts and their bytes
 *   stay separate from frames, work and matched. The digest and hash context
 *   live on the stack and are wiped on return.
 *
 * Work: one unit per storage comparison, TC_LDS_hash_find, the digest length
 * and the hashed bytes.
 * Returns OK and writes matched as 1 for an equal digest and 0 otherwise. END
 * when the group is absent. ARGUMENT for NULL arguments, a number outside
 * 1..16 or overlap. LIMIT for count above max_elements or a part list whose
 * storage checks exceed work, before any charge, and for exhausted limits,
 * frames or work later. UNSUPPORTED for a hash that this build disables.
 * Other statuses follow TC_LDS_hash_find. matched changes only on OK. */
TC_TLV_result TC_LDS_hash_check(const TC_LDS_security_object* object, unsigned number,
                                const TC_bytes* parts, size_t count, const TC_TLV_limits* limits,
                                TC_TLV_frames frames, size_t* work, int* matched);
#endif

#ifdef __cplusplus
}
#endif
#endif
