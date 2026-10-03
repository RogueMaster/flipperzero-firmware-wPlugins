/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV secure-messaging card verifiable certificates: the 7F21 reader and
 * chain verification under a validated content-signing certificate.
 * Standards: SP 800-73-5 Part 2 section 4.1.5.
 * Configuration: TC_ENABLE_PIV_CVC, with X.509 chain checks from
 * TC_ENABLE_X509.
 * Contracts: docs/api.md. Guide: docs/piv-cvc.md. */
#ifndef TINY_CRYPTO_PIV_CVC_H_
#define TINY_CRYPTO_PIV_CVC_H_
#include <tiny_crypto/der.h>
#include <tiny_crypto/ec.h>
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_CVC_CARD_APPLICATION = 0x00, TC_PIV_CVC_INTERMEDIATE = 0x12 } TC_PIV_CVC_role;

typedef struct {
  TC_bytes signed_data, issuer, subject, curve_oid, public_key;
  TC_DER_algorithm signature_algorithm;
  TC_bytes signature;
  TC_DER_signature_pair ecdsa;
  unsigned key_bits;
  uint8_t role;
} TC_PIV_CVC;

#if TC_ENABLE_PIV_CVC
/* Read one complete 7F21 card verifiable certificate with the fixed field
 * order of SP 800-73-5 Part 2 section 4.1.5. All spans borrow encoded, which
 * must stay unchanged while they are used. encoded and out must be disjoint.
 * signed_data is the original signed byte range. Charges no work.
 * Returns OK with out written. INVALID for truncated, malformed or trailing
 * input, a wrong tag or field size. LIMIT for a tag or length wider than ISO
 * 7816 allows. UNSUPPORTED for an unknown profile, curve, algorithm or role.
 * ARGUMENT for NULL out, NULL data with a length, or overlap. Errors leave
 * out unchanged. Signature verification and curve-membership checks are
 * separate. */
TC_TLV_result TC_PIV_CVC_read(TC_bytes encoded, TC_PIV_CVC* out);
#endif

typedef struct {
  TC_bytes card, intermediate, card_uuid;
  TC_EC_curve curve;
  /* Previously validated content-signing certificate, with its original bytes. */
  const TC_X509_certificate* signer;
} TC_PIV_CVC_chain_request;

#if TC_ENABLE_PIV_CVC && TC_ENABLE_X509
/* Verify the card CVC and optional intermediate under the supplied signer
 * (SP 800-73-5 Part 2 section 4.1.5). Check the signer's path,
 * content-signing usage, policy, time and revocation before calling, or use
 * TC_PIV_CVC_validate. The issuer links use the signer's
 * subjectKeyIdentifier. curve selects P-256 (CS2) or P-384 (CS7). A 16-byte
 * card_uuid binds the card identifier. An empty span skips that binding. The
 * intermediate's subject must equal the first 8 bytes of SHA-1 over its
 * public-key point. Both signatures cover the original signed TLVs. Subject
 * points are validated on the curve.
 * - Inputs stay borrowed and stable. work, point_workspace and out are
 *   disjoint from each other and from all inputs. The provider has separate
 *   scratch. point_workspace is cleared after each point check.
 *
 * Work: each CVC length, the subject key identifier scan, the intermediate's
 * point length plus 8, each signature charge described in docs/api.md, and 8
 * units per curve bit for each point check.
 * Returns VALID with out written. VALID covers this CVC chain under signer.
 * Secure messaging also requires key confirmation. ERROR for NULL arguments,
 * an empty card, a card_uuid of another nonzero length, or overlap, with all
 * state unchanged. UNSUPPORTED for a curve other than P-256 or P-384, a build
 * without EC, or a supplied intermediate without SHA-1, after the argument
 * checks and before any work or provider call. LIMIT for an input above
 * limits->max_input or exhausted work. INVALID for a malformed CVC, a wrong
 * role, issuer link or card identifier, a failed signature or a point off the
 * curve. Only VALID writes out. */
TC_X509_signature_result TC_PIV_CVC_chain_verify(const TC_PIV_CVC_chain_request* request,
                                                 const TC_TLV_limits* limits,
                                                 const TC_X509_signature_provider* provider,
                                                 TC_EC_workspace* point_workspace, size_t* work,
                                                 TC_PIV_CVC* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
