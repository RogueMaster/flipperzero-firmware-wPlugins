/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* EAC card verifiable certificates: certificate, public-key and extension
 * readers for the BSI TR-03110 profile, and issuer-dependent width checks.
 * Standards: BSI TR-03110 Part 3.
 * Configuration: TC_ENABLE_EAC_CVC, which requires TC_ENABLE_DER.
 * Limitations: readers parse encodings. Signature, key validity, trust and
 * expiration checks belong to the caller.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_EAC_CVC_H_
#define TINY_CRYPTO_EAC_CVC_H_
#include <tiny_crypto/der.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_EAC_RSA_V15, TC_EAC_RSA_PSS, TC_EAC_ECDSA, TC_EAC_ECDH } TC_EAC_algorithm;
typedef struct {
  TC_bytes oid, modulus, exponent, p, a, b, generator, order, point, cofactor;
  TC_EAC_algorithm algorithm;
  unsigned hash_bits;
  int has_domain;
} TC_EAC_CVC_public_key;
typedef struct {
  unsigned year;
  uint8_t month, day;
} TC_EAC_date;
typedef enum { TC_EAC_TERMINAL, TC_EAC_DV_FOREIGN, TC_EAC_DV_DOMESTIC, TC_EAC_CVCA } TC_EAC_role;
typedef enum { TC_EAC_IS = 1, TC_EAC_AT = 2, TC_EAC_ST = 3 } TC_EAC_terminal_type;
typedef struct {
  TC_bytes encoded, signed_data, issuer, holder, authorization_oid, authorization;
  TC_bytes extensions, signature;
  TC_EAC_CVC_public_key public_key;
  TC_EAC_date effective, expiration;
  TC_EAC_role role;
  TC_EAC_terminal_type terminal_type;
} TC_EAC_CVC;
/* Caller-owned nesting scratch. limits.max_depth frames always suffice. */
typedef struct {
  TC_TLV_frames frames;
} TC_EAC_CVC_workspace;

#if TC_ENABLE_EAC_CVC
/* Parse and bounds-check one complete TR-03110 certificate (tag 7F21) as
 * encoded (BSI TR-03110 Part 3 appendix C.1). Checks the profile identifier,
 * certification authority and holder references, the public key, the
 * certificate holder authorization template of an IS, AT or ST terminal, the
 * dates and their order, and the extension templates. Spans borrow encoded,
 * which must stay unchanged while they are used. limits bounds the whole
 * tree, and limits->max_depth frames always suffice. encoded, the workspace
 * frames and out must be disjoint. Charges no work.
 * Returns OK with out written. INVALID for truncated, malformed or trailing
 * input or a field outside the profile. LIMIT when limits or frame capacity
 * are exhausted. UNSUPPORTED for an unknown profile, key algorithm or
 * authorization role. ARGUMENT for NULL pointers, NULL data with a length, or
 * overlap. out is unchanged on failure and workspace frames may change. The
 * caller verifies the signature, chain and dates. */
TC_TLV_result TC_EAC_CVC_read(TC_bytes encoded, const TC_TLV_limits* limits,
                              const TC_EAC_CVC_workspace* workspace, TC_EAC_CVC* out);
/* Read one standalone public key (tag 7F49). A standalone key may also be
 * an EC key-agreement key, and EC domain parameters stay optional. Statuses,
 * lifetime and work follow TC_EAC_CVC_read without the frame workspace.
 * encoded and out must be disjoint. */
TC_TLV_result TC_EAC_CVC_public_key_read(TC_bytes encoded, const TC_TLV_limits* limits,
                                         TC_EAC_CVC_public_key* out);

/* Check the field widths that depend on another certificate: the subject EC
 * point against its domain prime and the signature against the issuer's
 * order or modulus. issuer must hold resolved parameters. inherited supplies
 * the subject's EC domain when its certificate omits it. The caller verifies
 * curve membership and the signature. Charges no work.
 * Returns OK. ARGUMENT for NULL certificate or issuer, a certificate without
 * a signature, or missing domain or issuer parameters. INVALID for a width
 * mismatch. UNSUPPORTED for an issuer key-agreement key. LIMIT for an
 * issuer order too wide to double in size_t. */
TC_TLV_result TC_EAC_CVC_check_encoding(const TC_EAC_CVC* certificate,
                                        const TC_EAC_CVC_public_key* issuer,
                                        const TC_EAC_CVC_public_key* inherited);
#endif

typedef struct {
  TC_bytes oid, fields;
} TC_EAC_CVC_extension;
#if TC_ENABLE_EAC_CVC
/* Start reading the extension templates (BSI TR-03110 Part 3 appendix C.3)
 * of certificate.extensions, or pass
 * {NULL, 0} when absent to get an empty reader. The iterator counts
 * templates, OIDs and immediate fields against limits->max_elements. Field
 * contents stay opaque. TC_EAC_CVC_read checks the full tree's depth and
 * element budgets. The reader borrows encoded. Charges no work.
 * Returns OK with the reader initialized. INVALID for a tag other than 65, an
 * empty template or trailing bytes. Other statuses follow TC_TLV_read and
 * TC_TLV_reader_init. The reader changes only on OK. */
TC_TLV_result TC_EAC_CVC_extensions_init(TC_TLV_reader* reader, TC_bytes encoded,
                                         const TC_TLV_limits* limits);
/* Read the next discretionary data template (tag 73): its OID and the
 * context-specific fields that follow. out borrows the reader input.
 * Returns OK with the reader advanced and out written. END when no template
 * remains. ARGUMENT for NULL arguments. LIMIT when max_elements runs out.
 * INVALID for a wrong tag, a malformed OID, no fields or a field outside the
 * context-specific class. The reader and out change only on OK. */
TC_TLV_result TC_EAC_CVC_extension_next(TC_TLV_reader* reader, TC_EAC_CVC_extension* out);
#endif
#ifdef __cplusplus
}
#endif
#endif
