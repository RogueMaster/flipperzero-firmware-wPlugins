/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key proofs: a PIV or TWIC card key signs a fresh random challenge with
 * GENERAL AUTHENTICATE, and the signature is verified under the key of its
 * validated certificate. The key policy selects the algorithm identifier
 * and the challenge encoding.
 * Standards: NIST SP 800-73-5 Part 1 Table 5, Part 2 3.2.4 and Appendix
 * A.4, NIST SP 800-78-5 section 3.1, Tables 9 and 10, TWIC Part 2 v5 5.3,
 * TWIC Part 3 v4 4.4.4.
 * Configuration: TC_ENABLE_PIV_KEY_PROOF (requires TC_ENABLE_PIV_COMMAND,
 * TC_ENABLE_KEY_CHALLENGE and TC_ENABLE_X509).
 * Limitations: the PIV Authentication (9A), Digital Signature (9C) and Card
 * Authentication (9E) keys only. Key management (9D), retired and symmetric
 * keys are outside this module. Certificate path, revocation and identifier
 * checks belong to the caller.
 * Contracts: docs/api.md.
 * Guide: docs/piv-card.md. */
#ifndef TINY_CRYPTO_PIV_KEY_PROOF_H_
#define TINY_CRYPTO_PIV_KEY_PROOF_H_
#include <tiny_crypto/key_challenge.h>
#include <tiny_crypto/piv_command.h>
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Algorithm identifiers of SP 800-78-5 Table 9 for the asymmetric keys. */
enum {
  TC_PIV_ALGORITHM_RSA_3072 = 0x05,
  TC_PIV_ALGORITHM_RSA_1024 = 0x06,
  TC_PIV_ALGORITHM_RSA_2048 = 0x07,
  TC_PIV_ALGORITHM_ECC_P256 = 0x11,
  TC_PIV_ALGORITHM_ECC_P384 = 0x14
};

/* Key references of SP 800-73-5 Part 1 Table 5 that sign challenges. */
enum {
  TC_PIV_KEY_PIV_AUTHENTICATION = 0x9a,
  TC_PIV_KEY_DIGITAL_SIGNATURE = 0x9c,
  TC_PIV_KEY_CARD_AUTHENTICATION = 0x9e
};

/* RSA challenge encoding: EMSA-PKCS1-v1_5, or EMSA-PSS with MGF1 and a
 * 32-byte salt, both over SHA-256 (SP 800-78-5 Table 2). */
typedef enum { TC_PIV_RSA_PKCS1_V15, TC_PIV_RSA_PSS } TC_PIV_rsa_padding;

/* Key policy.
 * profile        TC_PIV_CARD applies SP 800-78-5 Table 10. The TWIC
 *                profiles apply the TWIC reader policy.
 * at             the policy time, checked against the SP 800-78-5 Table 10
 *                end of RSA-2048 on 2030-12-31T23:59:59Z.
 * rsa_padding    a TC_PIV_rsa_padding value.
 * allow_rsa1024  1 permits RSA-1024 (06) on TC_TWIC_LEGACY_CARD, else 0. */
typedef struct {
  TC_PIV_card_profile profile;
  TC_X509_time at;
  TC_PIV_rsa_padding rsa_padding;
  uint8_t allow_rsa1024;
} TC_PIV_key_policy;

/* Proof parameters of one key: the challenge options for
 * TC_key_challenge_prepare and the Table 9 algorithm identifier. */
typedef struct {
  TC_key_challenge_options challenge;
  uint8_t algorithm;
} TC_PIV_key_parameters;

#if TC_ENABLE_PIV_KEY_PROOF
/* Select the proof parameters for the subject key of certificate under
 * policy. The certificate is one returned by TC_X509_read, usually after
 * path validation.
 *
 * - keyUsage must be present and include digitalSignature.
 * - RSA: an exponent of 65537 to 2^256 - 1 (SP 800-78-5 section 3.1), a
 *   modulus of exactly bits / 8 bytes, and 2048 (07) or 3072 (05) bits.
 *   Under TC_PIV_CARD, 2048 bits is accepted only through 2030 (Table 10).
 *   1024 bits (06) needs TC_TWIC_LEGACY_CARD and allow_rsa1024.
 * - ECDSA: P-256 (11) with SHA-256 or P-384 (14) with SHA-384, as an
 *   uncompressed point.
 * - TC_TWIC_NEXGEN_CARD accepts only RSA-2048, the 9E07 key of TWIC Part 2
 *   v5 4.5 and 5.3.
 * - RSA challenges use SHA-256 with the policy padding.
 *
 * TC_PIV_ARGUMENT     NULL pointer, an unknown profile or padding,
 *                     allow_rsa1024 other than 0 or 1, or an invalid time.
 * TC_PIV_INVALID      keyUsage absent or without digitalSignature, malformed
 *                     extensions, an exponent out of range or a modulus of
 *                     the wrong length.
 * TC_PIV_UNSUPPORTED  another key type, size or curve, or one the profile
 *                     or policy time excludes.
 *
 * out changes only on TC_PIV_OK. Charges no work. */
TC_PIV_result TC_PIV_key_parameters_select(const TC_X509_certificate* certificate,
                                           const TC_PIV_key_policy* policy,
                                           TC_PIV_key_parameters* out);
#endif

/* Command and answer sizes. The request is 7C {82 00, 81 L challenge} with
 * length fields of up to three octets (Part 2 A.4.1). The answer is 7C {82 L signature}: an
 * RSA signature of the modulus length or a DER ECDSA-Sig-Value of up to 104
 * bytes (P-384). */
#define TC_PIV_KEY_PROOF_REQUEST_BYTES ((size_t)TC_KEY_CHALLENGE_MAX_INPUT_BYTES + 12u)
#define TC_PIV_KEY_PROOF_SIGNATURE_BYTES                                                           \
  ((size_t)TC_KEY_CHALLENGE_MAX_INPUT_BYTES > 104u ? (size_t)TC_KEY_CHALLENGE_MAX_INPUT_BYTES      \
                                                   : 104u)
#define TC_PIV_KEY_PROOF_RESPONSE_BYTES TC_PIV_RESPONSE_BYTES(TC_PIV_KEY_PROOF_SIGNATURE_BYTES + 8u)

/* Storage for one proof. The members are private. */
typedef struct {
  TC_key_challenge_workspace challenge;
  uint8_t request[TC_PIV_KEY_PROOF_REQUEST_BYTES];
  uint8_t response[TC_PIV_KEY_PROOF_RESPONSE_BYTES];
} TC_PIV_key_proof_workspace;

/* One key proof.
 * certificate    the validated certificate of the key reference. Its bytes
 *                stay unchanged during the call.
 * policy         the key policy.
 * key_reference  9A, 9C or 9E on the PIV application, 9E on the TWIC
 *                application (TWIC Part 2 v5 5.3 syntax table). */
typedef struct {
  const TC_X509_certificate* certificate;
  TC_PIV_key_policy policy;
  uint8_t key_reference;
} TC_PIV_key_proof_request;

#if TC_ENABLE_PIV_KEY_PROOF
/* Prove that the card holds the private key of request->certificate.
 * TC_PIV_key_parameters_select chooses the parameters.
 * TC_key_challenge_prepare draws a fresh challenge from random. The command
 * is GENERAL AUTHENTICATE CLA 00, INS 87, P1 the algorithm, P2 the key
 * reference, data 7C {82 00, 81 L challenge} and the Le of the link: 00 on
 * a SHORT link (Part 2 3.2.4, A.4.1). The channel chains a data field above 255 bytes, and a secured
 * link sends the command under secure messaging with 1C fragments
 * (piv_sm_apdu.h). The answer must be exactly 7C {82 L signature}, and
 * TC_key_challenge_verify checks the signature.
 *
 * The TWIC application takes the profile of its SELECT and offers the proof
 * on NEXGEN cards only (TWIC Part 2 v5 5.3). The PIV application takes
 * TC_PIV_CARD, or a TWIC profile on a TWIC card's PIV application.
 * 9C needs a PIN verified immediately before the proof (PIN Always, Part 1
 * Table 5). The card enforces the PIN, and a missing PIN returns
 * TC_PIV_CARD_STATUS with 6982. TC_PIV_pin_verify submits the PIN only
 * while the card reports it unverified, so prove 9C directly after the PIN
 * submission of the card session. TWIC Part 3 v4 4.4.4 asks for challenges of
 * at least 127 bytes, which RSA encoded messages meet.
 *
 * TC_PIV_ARGUMENT     NULL pointer or random.fill, a cleared link, a key
 *                     reference or policy outside the rules above, the
 *                     request, the certificate and its key bytes, *provider
 *                     or *work inside *link or its scratch buffers, or a
 *                     workspace overlapping any of them.
 * TC_PIV_REFUSED      no application is selected, the link lost its secure
 *                     messaging session, or 9A or 9C on a contactless link
 *                     without the VCI (Part 1 Table 5). Nothing was sent.
 * TC_PIV_UNSUPPORTED  the TWIC application of a Legacy card, a provider
 *                     without verify_digest, a key policy result, or an
 *                     unsupported signature at verify.
 * TC_PIV_INVALID      a key policy result, a malformed answer, or a
 *                     signature that fails verification.
 * TC_PIV_CARD_STATUS  the card answered other than 9000, such as 6982 (PIN
 *                     or VCI needed) or 6A80. TC_PIV_link_status holds it.
 * TC_PIV_LIMIT        exhausted work, or a channel or secure messaging
 *                     limit.
 * TC_PIV_ERROR        the random source, provider or transport failed, or
 *                     the link is stopped.
 *
 * Only TC_PIV_OK means the key proved possession. ARGUMENT, REFUSED, the
 * Legacy TWIC application and a provider without verify_digest leave the
 * workspace and *work unchanged, send nothing and draw no randomness. Every
 * later return wipes the workspace. Work: the charges of
 * TC_key_challenge_prepare and TC_key_challenge_verify. The exchange budget
 * covers the chain fragments and the GET RESPONSE steps of the answer. */
TC_PIV_result TC_PIV_key_prove(TC_PIV_link* link, const TC_PIV_key_proof_request* request,
                               TC_random_source random, const TC_X509_signature_provider* provider,
                               TC_PIV_key_proof_workspace* workspace, TC_work_budget* work);
#endif

#ifdef __cplusplus
}
#endif
#endif
