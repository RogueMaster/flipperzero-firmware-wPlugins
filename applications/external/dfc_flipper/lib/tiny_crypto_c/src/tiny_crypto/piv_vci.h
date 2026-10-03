/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV virtual contact interface (VCI) on a secured TC_PIV_link: the
 * Discovery Object read over the link and the pairing-code VERIFY that
 * establishes the VCI.
 * Standards: NIST SP 800-73-5 Part 1 sections 3.3.2 and 5.5, Table 2
 * footnote 9 and Table 4. Part 2 sections 3.2.1, 3.2.1.3 and Appendix A.6.
 * Configuration: TC_ENABLE_PIV_VCI (requires TC_ENABLE_PIV_SM_APDU and
 * TC_ENABLE_PIV_OBJECTS).
 * Limitations: the library owns no pairing-code entry or storage. OCC is
 * outside this module.
 * Contracts: docs/api.md.
 * Guide: docs/piv-sm.md. */
#ifndef TINY_CRYPTO_PIV_VCI_H_
#define TINY_CRYPTO_PIV_VCI_H_
#include <tiny_crypto/piv_discovery.h>
#include <tiny_crypto/piv_sm_apdu.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Pairing code length (Part 2 section 2.4.3): exactly 8 ASCII digits. */
#define TC_PIV_PAIRING_CODE_DIGITS 8u
/* Response buffer bytes for the Discovery Object on any link: 20 bytes, or
 * 22 in the TWIC 53 form. */
#define TC_PIV_DISCOVERY_RESPONSE_BYTES TC_PIV_RESPONSE_BYTES(22u)

#if TC_ENABLE_PIV_VCI
/* Read the Discovery Object with GET DATA 7E on the selected application
 * and check it with TC_PIV_discovery_read under profile. A secured link
 * reads it under secure messaging, and out->secured records that. The VCI
 * and PIN-reference decisions rely on this read, since the object carries
 * no signature of its own (Part 1 section 3.3.2). The TWIC form 53 {7E ...}
 * is accepted on the TWIC application.
 *
 * TC_PIV_ARGUMENT     NULL link or out, a cleared link, an unknown profile,
 *                     response with NULL data or below 2 bytes, or response
 *                     overlapping *link, its scratch buffers or *out.
 * TC_PIV_REFUSED      no application is selected, or the link lost its
 *                     secure messaging session.
 * TC_PIV_UNSUPPORTED  the card answered an empty object (TWIC Part 2 v5
 *                     section 3.3.6).
 * TC_PIV_CARD_STATUS  another status, such as 6A82. On a secured link an
 *                     outer secure messaging status also ends the session.
 * TC_PIV_INVALID      malformed framing, or an object outside the profile.
 * TC_PIV_LIMIT, TC_PIV_ERROR
 *                     channel results, and the session-loss results of
 *                     piv_sm_apdu.h on a secured link.
 *
 * out->aid borrows response. out changes only on TC_PIV_OK. Every failure
 * after the argument checks wipes response. */
TC_PIV_result TC_PIV_discovery_get(TC_PIV_link* link, TC_PIV_discovery_profile profile,
                                   TC_buffer response, TC_PIV_discovery* out);
#endif

/* How the VCI was established. PAIRED sent the pairing code. WITHOUT_PAIRING
 * needed no command, since policy bit 3 is set (Part 1 section 5.5). */
typedef enum { TC_PIV_VCI_PAIRED, TC_PIV_VCI_WITHOUT_PAIRING } TC_PIV_vci_mode;

#if TC_ENABLE_PIV_VCI
/* Establish the VCI on a secured PIV link (Part 1 section 5.5 and Table 2
 * footnote 9): secure messaging, a Discovery Object with policy bit 4, and
 * either policy bit 3 or a pairing status of TRUE. discovery must come from
 * TC_PIV_discovery_get on this link while it was secured, under the PIV
 * profile.
 * - Policy bit 3 set: nothing is sent, and *out is WITHOUT_PAIRING.
 * - Otherwise VERIFY 00 20 00 98 goes out under secure messaging with the
 *   8-digit pairing_code (Part 2 sections 3.2.1 and 3.2.1.3, Appendix A.6).
 *   9000 gives PAIRED. The code is copied to a stack array and the secure
 *   messaging scratch, which are wiped.
 * On TC_PIV_OK the link reports vci = 1, which TC_PIV_pin_verify and later
 * layers use for the contactless rules of Part 1 Tables 2 and 4. The VCI
 * ends with the session: on SELECT, TC_PIV_link_unsecure, a session loss and
 * a new key request. The contact interface accepts the call too, where it
 * serves no purpose (Part 1 Table 4 footnote 11).
 *
 * TC_PIV_ARGUMENT     NULL link, discovery or out, a cleared link,
 *                     pairing_code with NULL data and a nonzero length, a
 *                     length other than 0 and 8, a non-digit, an empty code
 *                     when the policy requires pairing, or pairing_code
 *                     overlapping *link, its scratch buffers or *out.
 * TC_PIV_UNSUPPORTED  the TWIC application, a discovery read under the TWIC
 *                     profile, or policy bit 4 clear: the card has no VCI.
 * TC_PIV_REFUSED      no application is selected, the link is unsecured or
 *                     lost its session, or discovery was read without secure
 *                     messaging. Nothing was sent.
 * TC_PIV_CARD_STATUS  the card answered other than 9000, such as 6300
 *                     (wrong code), 6A80 or 6A88. The link reports vci = 0.
 *                     An inner status keeps the session READY. An outer
 *                     secure messaging status, such as 6988, ends the
 *                     session as piv_sm_apdu.h describes.
 * TC_PIV_INVALID, TC_PIV_LIMIT, TC_PIV_ERROR
 *                     the session-loss results of piv_sm_apdu.h. A LIMIT
 *                     found before protection keeps the session.
 *
 * *out changes only on TC_PIV_OK. */
TC_PIV_result TC_PIV_vci_establish(TC_PIV_link* link, const TC_PIV_discovery* discovery,
                                   TC_bytes pairing_code, TC_PIV_vci_mode* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
