/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Signer and profile policy for the credential validators. The helpers read
 * the signer certificate and set caller-owned path options before the
 * validators verify the object signature and the signer path. */
#ifndef TC_CREDENTIAL_POLICY_INTERNAL_H_
#define TC_CREDENTIAL_POLICY_INTERNAL_H_

#include <tiny_crypto/credential.h>
#include <tiny_crypto/x509_path.h>

/* Resolve the card profile. Sets *piv for PIV cards and *oids to the OID
 * profile the card accepts: PIV OIDs only for PIV, PIV or TWIC OIDs for TWIC
 * Legacy and NEXGEN. Returns 1 when options->certificate.purpose is empty or
 * names a content-signing OID accepted by that profile. Returns 0 for an
 * unknown profile, NULL arguments or any other requested purpose. */
int tc_credential_profile(TC_PIV_card_profile profile, const TC_validation_options* options,
                          int* piv, TC_PIV_oid_profile* oids);

/* Parse a content signer certificate with storage frames and OIDs, which
 * are overwritten. Charges one work unit per certificate byte. The returned
 * view borrows certificate and stays usable after storage is reused. */
TC_TLV_result tc_credential_signer_read(TC_bytes certificate, const TC_TLV_limits* limits,
                                        const TC_X509_path_workspace* storage, size_t* work,
                                        TC_X509_certificate* out);

/* Configure policy for a parsed content signer in one extension pass. When
 * policy->purpose is empty, or the card is TWIC, the first content-signing EKU
 * in the signer becomes the purpose. TWIC-compatible matching also accepts the
 * TWIC OID. PIV cards (piv != 0) additionally require the PIV content-signing
 * certificate policy, set it as the explicit initial policy, and reject
 * signers that expire before card_expiration when it is non-NULL. Every
 * profile requires digitalSignature keyUsage and a present EKU with
 * anyExtendedKeyUsage inhibited.
 *
 * The scan charges the extension bytes, one unit per extension and the value
 * bytes of certificatePolicies and EKU. Exhaustion returns TC_TLV_LIMIT.
 * storage OIDs are overwritten. policy->purpose borrows signer bytes and
 * policy->initial_policies borrows static data. policy changes only on
 * TC_TLV_OK. Returns TC_TLV_INVALID when the signer lacks the required EKU or
 * policy. */
TC_TLV_result tc_credential_signer_policy(const TC_X509_certificate* signer, int piv,
                                          int twic_compatible, const TC_X509_time* card_expiration,
                                          TC_X509_path_options* policy,
                                          const TC_X509_path_workspace* storage, size_t* work);

/* Compare a CHUID expiration date (YYYYMMDD) with at. The card is valid
 * through 23:59:59 UTC on that day, so *valid is 1 when at is on or before
 * that time. Returns TC_TLV_ARGUMENT for NULL arguments or a length other
 * than 8, and TC_TLV_INVALID for an invalid Gregorian calendar date.
 * *valid changes only on TC_TLV_OK. */
TC_TLV_result tc_credential_chuid_expiration_check(TC_bytes expiration, const TC_X509_time* at,
                                                   int* valid);

#endif
