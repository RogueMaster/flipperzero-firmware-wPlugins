/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_OBJECT_H_
#define EXAMPLE_CREDENTIAL_OBJECT_H_
#include "cms_validate.h"
#include <tiny_crypto/piv_cvc.h>
#include <tiny_crypto/credential.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_LDS_CONTENT_CAPACITY = 2048 };
typedef struct {
  ExampleCMSCredentialWorkspace credential;
  uint8_t content[EXAMPLE_LDS_CONTENT_CAPACITY];
} ExampleSecurityWorkspace;

/* Example adapters retain the fixed-size demonstration workspaces. Production
 * callers can use the public request and workspace views directly. */
typedef TC_PIV_biometric_validation_request ExampleBiometricRequest;
typedef TC_PIV_security_data ExampleSecurityData;
typedef TC_PIV_security_validation_request ExampleSecurityRequest;
/* Adapt the older path/revocation option pair used by focused examples. The
 * generic API requires one evaluation time and signature provider. */
TC_result example_validation_options(const TC_CMS_path_options* path,
    const TC_CMS_revocation_policy* revocation, TC_validation_options* out);
TC_credential_status example_validate_biometric(
    const TC_PIV_biometric_validation_request* request,
    const TC_X509_store_snapshot* snapshot, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation, size_t* work,
    ExampleCMSCredentialWorkspace* workspace);
TC_credential_status example_validate_security(
    const TC_PIV_security_validation_request* request,
    const TC_X509_store_snapshot* snapshot, const TC_CMS_path_options* options,
    const TC_CMS_revocation_policy* revocation, size_t* work,
    ExampleSecurityWorkspace* workspace);

typedef struct {
  TC_bytes card, intermediate, expected_uuid, signer_certificate;
  TC_EC_curve curve;
  TC_PIV_card_profile profile;
} ExampleCVCRequest;

enum { EXAMPLE_CVC_ARENA_STORAGE = 1024 };
typedef struct {
  union {
    TC_validation_storage arena[EXAMPLE_CVC_ARENA_STORAGE];
    TC_EC_workspace point;
  } scratch;
  TC_validation_workspace validation;
} ExampleCVCCredentialWorkspace;

/* Authenticate the CVC signer through a held trust snapshot and CRL index,
 * then verify the card/intermediate chain. PIV requires its registered content
 * signing policy and EKU; TWIC keeps the caller's certificate-policy settings.
 * options and revocation use the same evaluation time. Hold all source bytes,
 * policies and the acquired snapshot stable through the acceptance decision.
 * Inputs, output, work, workspace and provider scratch are disjoint. The arena
 * and EC point scratch share storage across sequential phases. Processing clears
 * workspace. Only VALID writes out, borrowing the card's original bytes.
 * Complete secure-messaging key confirmation before accepting the session. */
TC_credential_status example_validate_cvc(const ExampleCVCRequest* request,
    const TC_X509_store_snapshot* snapshot, const TC_X509_path_options* options,
    const TC_X509_revocation_options* revocation, size_t* work,
    ExampleCVCCredentialWorkspace* workspace, TC_PIV_CVC* out);

#ifdef __cplusplus
}
#endif
#endif
