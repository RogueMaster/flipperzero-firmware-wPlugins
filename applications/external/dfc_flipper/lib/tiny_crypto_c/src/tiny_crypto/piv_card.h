/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_CARD_H_
#define TINY_CRYPTO_PIV_CARD_H_
#include <tiny_crypto/piv_oid.h>
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_CARD,
  TC_TWIC_LEGACY_CARD,
  TC_TWIC_NEXGEN_CARD
} TC_PIV_card_profile;

typedef struct {
  TC_bytes fascn, fascn_oid, uuid_urn, cardholder_uuid_urn;
} TC_PIV_card_identifiers;

/* Read the DER GeneralNames value of a card-authentication certificate's SAN.
 * The profile selects accepted OIDs and UUID rules. Spans borrow the input.
 * Parsing can consume frames and work; out changes only on OK. Keep writable
 * storage disjoint from the input. */
TC_TLV_result TC_PIV_card_identifiers_read(TC_bytes subject_alt_name,
                                           TC_PIV_card_profile profile,
                                           const TC_TLV_limits *limits,
                                           TC_TLV_frame *frames,
                                           size_t frame_capacity, size_t *work,
                                           TC_PIV_card_identifiers *out);

/* Compare a successfully read identifier view with a CHUID's FASC-N and GUID.
 * Inputs may overlap. Keep work and matched disjoint from all inputs and each
 * other. Errors preserve matched; storage errors also preserve work. */
TC_TLV_result
TC_PIV_card_identifiers_match(const TC_PIV_card_identifiers *identifiers,
                              TC_bytes fascn, TC_bytes guid, size_t *work,
                              int *matched);

/* TWIC Part 3 section 4.4.4 reader policy: require the signed FASC-N and allow
 * an absent UUID. The selected TWIC profile validates any UUID present.
 * Ownership and failure behavior match TC_PIV_card_identifiers_read. */
TC_TLV_result TC_TWIC_card_identifiers_read(TC_bytes subject_alt_name,
                                            TC_PIV_card_profile profile,
                                            const TC_TLV_limits *limits,
                                            TC_TLV_frame *frames,
                                            size_t frame_capacity, size_t *work,
                                            TC_PIV_card_identifiers *out);

/* Read identifiers from a PIV Authentication certificate. card_guid selects
 * the required Card UUID when the SAN also carries a Cardholder UUID. The
 * optional Cardholder UUID must be version 4. Spans borrow subject_alt_name. */
TC_TLV_result TC_PIV_authentication_identifiers_read(
    TC_bytes subject_alt_name, TC_bytes card_guid, const TC_TLV_limits *limits,
    TC_TLV_frame *frames, size_t frame_capacity, size_t *work,
    TC_PIV_card_identifiers *out);

/* Apply TWIC reader policy to a PIV Authentication certificate. Registered PIV
 * and TWIC FASC-N OIDs are accepted. The Card UUID may be absent; any UUIDs
 * present follow the PIV Authentication selection rules above. */
TC_TLV_result TC_TWIC_authentication_identifiers_read(
    TC_bytes subject_alt_name, TC_bytes card_guid, const TC_TLV_limits *limits,
    TC_TLV_frame *frames, size_t frame_capacity, size_t *work,
    TC_PIV_card_identifiers *out);

/* Bind authenticated TWIC objects by their complete FASC-N. A supplied UUID
 * must also match the CHUID GUID. Ownership and failure behavior match
 * TC_PIV_card_identifiers_match. */
TC_TLV_result
TC_TWIC_card_identifiers_match(const TC_PIV_card_identifiers *identifiers,
                               TC_bytes fascn, TC_bytes guid, size_t *work,
                               int *matched);

#ifdef __cplusplus
}
#endif
#endif
