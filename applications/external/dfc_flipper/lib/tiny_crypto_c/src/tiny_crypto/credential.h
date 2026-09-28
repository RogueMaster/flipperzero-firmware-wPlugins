/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_CREDENTIAL_H_
#define TINY_CRYPTO_CREDENTIAL_H_

#include <tiny_crypto/cms.h>
#include <tiny_crypto/lds.h>
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/piv_chuid.h>
#include <tiny_crypto/piv_cms.h>
#include <tiny_crypto/piv_security.h>
#include <tiny_crypto/validation.h>
#if TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_cvc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_bytes encoded;
  TC_PIV_CHUID_encoding encoding;
  TC_PIV_card_profile profile;
  TC_PIV_CHUID_profile chuid_profile;
  /* Accept registered TWIC aliases and reader identifier rules for a PIV app.
   */
  int twic_reader_policy;
  /* Identifiers and expiration from the already validated card certificate. */
  const TC_PIV_card_identifiers *card;
  const TC_X509_time *card_expiration;
} TC_PIV_CHUID_validation_request;

typedef struct {
  TC_PIV_CHUID object;
  TC_bytes signer;
  TC_X509_time at;
  TC_PIV_card_profile profile;
} TC_PIV_CHUID_result;

/* Authenticate a signed CHUID and bind its identifiers to the validated card.
 * Choose the card OID policy and CHUID schema explicitly. Expiration includes
 * the final second of its UTC date. On VALID, out borrows CHUID and signer
 * bytes. Keep writable state disjoint from inputs. */
TC_credential_status
TC_PIV_CHUID_validate(const TC_PIV_CHUID_validation_request *request,
                      const TC_validation_context *context, size_t *work,
                      TC_PIV_CHUID_result *out);

typedef struct {
  /* Complete BC value, after any outer TWIC privacy-key decryption. */
  TC_bytes encoded;
  TC_PIV_card_profile profile;
  /* Borrowed from the authenticated CHUID and its signing certificate. */
  TC_bytes fascn, guid, chuid_signer;
  const TC_X509_time *card_expiration;
  /* Select the current or legacy biometric CMS profile explicitly. */
  TC_PIV_CMS_kind signature_profile;
  TC_PIV_CBEFF_format format;
  /* Set to one to require the CBEFF validity period at context time. */
  int require_current;
} TC_PIV_biometric_validation_request;

/* Authenticate a biometric object's CBEFF header and record and bind both
 * identifiers to an authenticated CHUID. An omitted CMS certificate selects
 * chuid_signer. Inputs remain borrowed. VALID covers object authentication and
 * identifier binding and the selected record profile. */
TC_credential_status
TC_PIV_biometric_validate(const TC_PIV_biometric_validation_request *request,
                          const TC_validation_context *context, size_t *work);

typedef struct {
  uint16_t container;
  const TC_bytes *parts;
  size_t count;
} TC_PIV_security_data;

typedef struct {
  TC_bytes encoded;
  TC_PIV_security_encoding encoding;
  TC_PIV_card_profile profile;
  TC_bytes chuid_signer;
  const TC_X509_time *card_expiration;
  /* Complete inventory. Container IDs must be unique; each object has parts. */
  const TC_PIV_security_data *objects;
  size_t count;
} TC_PIV_security_validation_request;

typedef struct {
  /* Decoded LDS content scratch. Capacity is bounded by the application. */
  uint8_t *content;
  size_t content_capacity;
} TC_PIV_security_validation_workspace;

/* Inventory descriptors and their bytes remain borrowed and immutable through
 * subsequent checks. This result survives reuse of the validation workspace. */
typedef struct {
  const TC_PIV_security_data *objects;
  size_t count;
  TC_bytes signer;
  TC_PIV_card_profile profile;
  TC_X509_time at;
} TC_PIV_security_result;

/* Authenticate a Security Object with the CHUID signer, then check the exact
 * inventory against signed LDS digests. Parts supply each object's bytes in
 * hash order. All buffers remain caller-owned; content is disjoint mutable
 * scratch. On VALID, out borrows the inventory and signer bytes. */
TC_credential_status
TC_PIV_security_validate(const TC_PIV_security_validation_request *request,
                         const TC_validation_context *context,
                         const TC_PIV_security_validation_workspace *workspace,
                         size_t *work, TC_PIV_security_result *out);

enum { TC_TWIC_UNSIGNED_CHUID_CONTAINER = 0x3002 };

typedef struct {
  TC_bytes encoded;
  TC_PIV_CHUID_encoding encoding;
  TC_PIV_card_profile profile;
  const TC_PIV_card_identifiers *card;
} TC_TWIC_unsigned_CHUID_validation_request;

/* Check an unsigned CHUID against a previously authenticated inventory at the
 * same evaluation time. Container 3002 must match encoded byte for byte.
 * The inventory and inputs remain borrowed. Keep work disjoint from them. */
TC_credential_status TC_TWIC_unsigned_CHUID_validate(
    const TC_TWIC_unsigned_CHUID_validation_request *chuid,
    const TC_PIV_security_result *security,
    const TC_validation_context *context, size_t *work);

#if TC_ENABLE_PIV_CVC
typedef struct {
  TC_bytes card, intermediate, expected_uuid, signer_certificate;
  TC_EC_curve curve;
  TC_PIV_card_profile profile;
} TC_PIV_CVC_validation_request;

/* Validate the X.509 signer's path and CRLs, then authenticate the CVC chain.
 * The card profile selects compatible OIDs. On VALID, out borrows card bytes.
 * Keep point scratch disjoint from inputs, provider state, work and out. */
TC_credential_status
TC_PIV_CVC_validate(const TC_PIV_CVC_validation_request *request,
                    const TC_validation_context *context,
                    TC_EC_workspace *point, size_t *work, TC_PIV_CVC *out);
#endif

#ifdef __cplusplus
}
#endif
#endif
