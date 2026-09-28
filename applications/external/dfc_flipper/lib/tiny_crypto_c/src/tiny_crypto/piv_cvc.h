/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_CVC_H_
#define TINY_CRYPTO_PIV_CVC_H_
#include <tiny_crypto/der.h>
#include <tiny_crypto/ec.h>
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_CVC_CARD_APPLICATION = 0x00,
  TC_PIV_CVC_INTERMEDIATE = 0x12
} TC_PIV_CVC_role;

typedef struct {
  TC_bytes signed_data, issuer, subject, curve_oid, public_key;
  TC_DER_algorithm signature_algorithm;
  TC_bytes signature;
  TC_DER_signature_pair ecdsa;
  unsigned key_bits;
  uint8_t role;
} TC_PIV_CVC;

/* Pass one complete 7F21 object. All spans borrow input; input and out must be
 * disjoint. Errors preserve out. signed_data is the original signed byte range.
 * Signature verification and curve-membership checks are separate operations. */
TC_TLV_result TC_PIV_CVC_read(const uint8_t* data, size_t length, TC_PIV_CVC* out);

typedef struct {
  TC_bytes card, intermediate, card_uuid;
  TC_EC_curve curve;
  /* Previously validated content-signing certificate, with its original bytes. */
  const TC_X509_certificate* signer;
} TC_PIV_CVC_chain_request;

/* Verify the card CVC and optional intermediate under the supplied signer.
 * Check the signer's path, content-signing usage, policy, time and revocation
 * before calling. The issuer links use its subjectKeyIdentifier. curve selects
 * P-256 (CS2) or P-384 (CS7). A supplied card_uuid binds the 16-byte card
 * identifier; an empty span discovers the identifier from the verified CVC.
 * The intermediate's subject is checked against its public-key SHA-1 prefix.
 * Both signatures use the original signed TLVs. Subject points are validated.
 *
 * Inputs remain borrowed and stable. work, point_workspace and out are disjoint
 * from each other and all inputs. Provider context has separate scratch.
 * Argument failures preserve caller state; processing consumes bounded work.
 * Only VALID writes out. Point scratch is cleared after use. VALID covers this
 * CVC chain under signer; secure messaging also requires key confirmation.
 * Requires X509, PIV_CVC and the selected EC curve; intermediates require SHA-1. */
TC_X509_signature_result TC_PIV_CVC_chain_verify(const TC_PIV_CVC_chain_request* request,
    const TC_TLV_limits* limits, const TC_X509_signature_provider* provider,
    TC_EC_workspace* point_workspace, size_t* work, TC_PIV_CVC* out);

#ifdef __cplusplus
}
#endif
#endif
