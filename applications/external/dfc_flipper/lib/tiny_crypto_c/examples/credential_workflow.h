/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_WORKFLOW_H_
#define EXAMPLE_CREDENTIAL_WORKFLOW_H_

#include "card_key_policy.h"
#include <tiny_crypto/credential.h>
#include <tiny_crypto/piv_printed.h>
#include <tiny_crypto/twic_ccl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EXAMPLE_CREDENTIAL_VALID,
  EXAMPLE_CREDENTIAL_INVALID,
  EXAMPLE_CREDENTIAL_REVOKED,
  EXAMPLE_CREDENTIAL_CANCELLED,
  EXAMPLE_CREDENTIAL_STALE,
  EXAMPLE_CREDENTIAL_UNAVAILABLE,
  EXAMPLE_CREDENTIAL_UNSUPPORTED,
  EXAMPLE_CREDENTIAL_LIMIT,
  EXAMPLE_CREDENTIAL_PROOF_FAILED,
  EXAMPLE_CREDENTIAL_ERROR
} ExampleCredentialVerdict;

typedef enum {
  EXAMPLE_CREDENTIAL_REQUIRE_SECURITY = 1u << 0,
  EXAMPLE_CREDENTIAL_REQUIRE_UNSIGNED_CHUID = 1u << 1,
  EXAMPLE_CREDENTIAL_REQUIRE_PRINTED = 1u << 2,
  EXAMPLE_CREDENTIAL_REQUIRE_FINGERPRINT = 1u << 3,
  EXAMPLE_CREDENTIAL_REQUIRE_FACE = 1u << 4,
  EXAMPLE_CREDENTIAL_REQUIRE_IRIS = 1u << 5
} ExampleCredentialRequirement;

typedef enum {
  EXAMPLE_CREDENTIAL_CARD_AUTHENTICATION,
  EXAMPLE_CREDENTIAL_PIV_AUTHENTICATION
} ExampleCredentialCardKey;

/* Perform a fresh card-key proof at the application transport boundary.
 * TC_MISMATCH rejects the card; other nonzero values report an API/transport
 * failure. The wrapper enforces the profile's key algorithm policy before the
 * callback and supplies the selected key reference and challenge policy. The
 * callback must consume no validation scratch. */
typedef TC_status (*ExampleCredentialProof)(
    void *context, TC_PIV_card_profile profile,
    ExampleCredentialCardKey key_reference, const TC_X509_public_key *card_key,
    const TC_key_challenge_options *challenge);

typedef struct {
  TC_bytes encoded;
  TC_PIV_security_encoding encoding;
  const TC_PIV_security_data *objects;
  size_t count;
  /* Optional exact unsigned CHUID represented by container 3002. */
  TC_bytes unsigned_chuid;
  TC_PIV_CHUID_encoding unsigned_chuid_encoding;
  TC_buffer content;
  /* Decrypted DFC109 contents, when included in objects as container 3001. */
  TC_bytes printed;
} ExampleCredentialSecurityInput;

typedef struct {
  TC_bytes encoded;
  TC_PIV_CMS_kind signature_profile;
  TC_PIV_CBEFF_format format;
  int require_current;
} ExampleCredentialBiometricInput;

typedef struct {
  TC_PIV_card_profile profile;
  ExampleCredentialCardKey card_key;
  /* Apply TWIC reader identifier policy to a PIV Authentication certificate. */
  int twic_reader_policy;
  TC_bytes certificate, chuid;
  TC_PIV_CHUID_encoding chuid_encoding;
  /* Select the signed CHUID schema used by this application. */
  TC_PIV_CHUID_profile chuid_profile;
  /* TWIC requires a held CCL and freshness policy. PIV leaves these zero. */
  const TC_TWIC_CCL_snapshot *ccl;
  TC_TWIC_CCL_freshness_policy freshness;
  size_t ccl_reads;
  int allow_legacy_rsa1024;
  /* RSA representative encoding for the fresh proof. EC ignores this field. */
  ExampleCardRSAPadding rsa_padding;
  ExampleCredentialProof proof;
  void *proof_context;
  /* Application policy for evidence that must be present in this decision. */
  unsigned required_objects;
  /* Empty encoded spans omit these dependent objects. */
  ExampleCredentialSecurityInput security;
  /* Each format may appear once. The list is bounded to three modalities. */
  const ExampleCredentialBiometricInput *biometrics;
  size_t biometric_count;
} ExampleCredentialValidationRequest;

typedef struct {
  TC_X509_validation_result card;
  TC_PIV_card_identifiers identifiers;
  TC_PIV_CHUID_result chuid;
  TC_PIV_security_result security;
  int has_security;
  TC_PIV_printed printed;
  int has_printed;
} ExampleCredentialValidationResult;

/* Compose validation over retained PIV or TWIC objects. The card and content
 * contexts may share an arena because every phase is sequential. Their
 * evaluation times must match. TWIC also requires freshness.now to equal that
 * time. A successful result borrows request and trust bytes. Repeat validation
 * with current policy and evidence before a later access decision. */
ExampleCredentialVerdict
example_credential_validate(const ExampleCredentialValidationRequest *request,
                            const TC_validation_context *card_context,
                            const TC_validation_context *content_context,
                            size_t *work,
                            ExampleCredentialValidationResult *out);

#ifdef __cplusplus
}
#endif
#endif
