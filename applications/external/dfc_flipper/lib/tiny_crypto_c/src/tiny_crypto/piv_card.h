/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Card identifiers from the card-authentication certificate: FASC-N, card
 * UUID and cardholder UUID in the subjectAltName, per PIV or TWIC profile.
 * Standards: FIPS 201-3, SP 800-73-5 Part 1, TWIC Part 2 v5.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Contracts: docs/api.md, including its size_t work units. */
#ifndef TINY_CRYPTO_PIV_CARD_H_
#define TINY_CRYPTO_PIV_CARD_H_
#include <tiny_crypto/piv_oid.h>
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_CARD, TC_TWIC_LEGACY_CARD, TC_TWIC_NEXGEN_CARD } TC_PIV_card_profile;

typedef struct {
  TC_bytes fascn, fascn_oid, uuid_urn, cardholder_uuid_urn;
} TC_PIV_card_identifiers;

#if TC_ENABLE_PIV_OBJECTS
/* Read the FASC-N and card UUID from the DER GeneralNames value of a Card
 * Authentication certificate's subjectAltName (SP 800-73-5 Part 1 sections
 * 3.1.4 and 3.4.1, TWIC Part 2 v5 section 6).
 * - profile selects the accepted FASC-N OIDs and UUID rules. PIV accepts the
 *   PIV FASC-N OID and a version 1, 4 or 5 RFC 4122 UUID. TWIC profiles also
 *   accept the TWIC OID. TWIC Legacy requires the nil UUID, which may be
 *   absent. TWIC NEXGEN requires a UUID that encodes the FASC-N.
 * - Exactly one FASC-N and at most one urn:uuid URI are allowed. Other names
 *   are skipped. The FASC-N must decode with TC_FASCN_read.
 * Spans borrow subject_alt_name. subject_alt_name, limits, frames, work and
 * out must be disjoint.
 *
 * Work: a 10-unit storage check, the framing scan, twice each name's bytes,
 * the FASC-N OID bytes, URI prefixes, UUID text, 25 units for the FASC-N and
 * 16 units for a NEXGEN UUID check.
 * Returns OK with out written. ARGUMENT for NULL limits, work or out, an
 * unknown profile or overlap, with all state unchanged. LIMIT for exhausted
 * limits, frames or work. INVALID for bad framing, an empty sequence, a
 * missing, repeated or foreign-OID FASC-N, a bad or extra UUID, a missing
 * UUID where the profile requires one, or a NEXGEN UUID that differs from
 * the FASC-N. out changes only on OK. Frames and work are provisional on
 * failure. */
TC_TLV_result TC_PIV_card_identifiers_read(TC_bytes subject_alt_name, TC_PIV_card_profile profile,
                                           const TC_TLV_limits* limits, TC_TLV_frames frames,
                                           size_t* work, TC_PIV_card_identifiers* out);

/* Compare identifiers from a successful read with a CHUID's 25-byte FASC-N
 * and 16-byte GUID. An absent card UUID compares as the nil UUID. The inputs
 * may overlap each other. work and matched must be disjoint from them and
 * from each other.
 *
 * Work: one unit per storage comparison, 41 units, and the UUID text length.
 * Returns OK and writes matched as 1 when both identifiers match and 0
 * otherwise. ARGUMENT for NULL arguments, a FASC-N or GUID of the wrong
 * length, or overlap, with work and matched unchanged. INVALID for an
 * identifier view without a 25-byte FASC-N or with bad UUID text. LIMIT for
 * exhausted work. Errors leave matched unchanged. */
TC_TLV_result TC_PIV_card_identifiers_match(const TC_PIV_card_identifiers* identifiers,
                                            TC_bytes fascn, TC_bytes guid, size_t* work,
                                            int* matched);

/* TWIC Part 3 section 4.4.4 reader policy for a Card Authentication
 * certificate: require the signed FASC-N and allow an absent UUID. The
 * selected TWIC profile validates any UUID present. Ownership, work, status
 * and failure behavior match TC_PIV_card_identifiers_read. ARGUMENT also
 * covers the PIV profile. */
TC_TLV_result TC_TWIC_card_identifiers_read(TC_bytes subject_alt_name, TC_PIV_card_profile profile,
                                            const TC_TLV_limits* limits, TC_TLV_frames frames,
                                            size_t* work, TC_PIV_card_identifiers* out);

/* Read identifiers from a PIV Authentication certificate (SP 800-73-5 Part 1
 * sections 3.1.3, 3.4.1 and 3.4.2). card_guid is the 16-byte Card UUID from
 * the authenticated CHUID. The SAN may carry the Card UUID and a Cardholder
 * UUID in either order. Exactly one UUID must equal card_guid, and the
 * optional Cardholder UUID must be version 4. Only the PIV FASC-N OID is
 * accepted. card_guid is copied at entry. Spans borrow subject_alt_name.
 * Work, status and failure behavior match TC_PIV_card_identifiers_read.
 * ARGUMENT also covers a card_guid without 16 bytes, and INVALID covers a
 * SAN without exactly one UUID equal to card_guid. */
TC_TLV_result TC_PIV_authentication_identifiers_read(TC_bytes subject_alt_name, TC_bytes card_guid,
                                                     const TC_TLV_limits* limits,
                                                     TC_TLV_frames frames, size_t* work,
                                                     TC_PIV_card_identifiers* out);

/* Apply TWIC reader policy to a PIV Authentication certificate. Registered
 * PIV and TWIC FASC-N OIDs are accepted and the Card UUID may be absent. Any
 * UUIDs present follow the selection rules of
 * TC_PIV_authentication_identifiers_read, as do ownership, work, statuses and
 * failure behavior. */
TC_TLV_result TC_TWIC_authentication_identifiers_read(TC_bytes subject_alt_name, TC_bytes card_guid,
                                                      const TC_TLV_limits* limits,
                                                      TC_TLV_frames frames, size_t* work,
                                                      TC_PIV_card_identifiers* out);

/* Bind authenticated TWIC objects by their complete FASC-N. A card UUID in
 * the identifiers must also match the CHUID GUID. An absent one matches any
 * GUID. Ownership, work, statuses and failure behavior match
 * TC_PIV_card_identifiers_match. */
TC_TLV_result TC_TWIC_card_identifiers_match(const TC_PIV_card_identifiers* identifiers,
                                             TC_bytes fascn, TC_bytes guid, size_t* work,
                                             int* matched);
#endif

#ifdef __cplusplus
}
#endif
#endif
