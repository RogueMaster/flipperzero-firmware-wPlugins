/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV secure messaging on a TC_PIV_link: the key establishment command, link
 * securing and the 87/97/99/8E wire format of protected commands and
 * responses. TC_PIV_SM performs the cryptography.
 * Standards: NIST SP 800-73-5 Part 2 sections 4.1.1, 4.1.8 and 4.2 to 4.3,
 * Table 19 and footnotes 20, 22 and 25. ISO/IEC 7816-4:2020 5.3.3.
 * Configuration: TC_ENABLE_PIV_SM_APDU (requires TC_ENABLE_PIV_COMMAND,
 * TC_ENABLE_PIV_SM and TC_ENABLE_PIV_CVC).
 * Limitations: SHORT length fields with 1C command chaining only. Extended
 * length secure messaging, ISO/IEC 7816-4 and GlobalPlatform secure
 * messaging are outside this module. Card authentication of the peer CVC is
 * TC_PIV_SM_authenticate_response in piv_sm_authenticate.h.
 * Contracts: docs/api.md.
 * Guide: docs/piv-sm.md. */
#ifndef TINY_CRYPTO_PIV_SM_APDU_H_
#define TINY_CRYPTO_PIV_SM_APDU_H_
#include <tiny_crypto/piv_command.h>
#include <tiny_crypto/piv_sm.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Largest plain command data field under secure messaging: the SM data field
 * (87 with a 3-octet length, 97 01 00, 8E 08 tag) stays within Nc 65535. */
#define TC_PIV_SM_MAX_PLAIN_NC 65503u
/* Upper bound for the SM data field of a command with nc plain data bytes:
 * 87 tag, length and indicator (5) with the padded ciphertext, 97 01 00 and
 * 8E 08 tag. Valid for nc <= TC_PIV_SM_MAX_PLAIN_NC. */
#define TC_PIV_SM_COMMAND_DATA_BYTES(nc) (((nc) ? TC_PIV_PADDED_BYTES(nc) + 5u : 0u) + 3u + 10u)
/* Largest card CVC C_ICC (Part 2 Table 19, CS7): 7F21 82 LL LL, 5F29 (4),
 * 42 (10), 5F20 (19), 7F49 81 6A {06 05 OID, 86 61 point} (110), 5F4C (4) and
 * 5F37 79 {SEQUENCE algorithm, BIT STRING ECDSA P-384} (124). */
#define TC_PIV_SM_CARD_CVC_MAX_BYTES 275u
/* Largest key establishment answer: 7C 82 LL LL, 82 82 LL LL, CB_ICC, the
 * CS7 nonce (24), the cryptogram (16), C_ICC and SW1 SW2. */
#define TC_PIV_SM_KEY_RESPONSE_BYTES                                                               \
  (4u + 4u + 1u + 24u + 16u + TC_PIV_SM_CARD_CVC_MAX_BYTES + TC_APDU_STATUS_BYTES)

#if TC_ENABLE_PIV_SM_APDU
/* Start key establishment on the selected PIV application and bind session
 * to the link (Part 2 4.1.1 steps H1 to H3, 4.1.8). The call runs
 * TC_PIV_SM_begin, sends GENERAL AUTHENTICATE CLA 00 INS 87 P1 suite P2 04
 * with 7C {81 {00 || host_id || Q_eH}, 82 00} and Le 00 in plaintext, and
 * decodes the answer 7C {82 {CB_ICC || N_ICC || AuthCryptogram_ICC ||
 * C_ICC}}. C_ICC must be one complete 7F21 DO of at most
 * TC_PIV_SM_CARD_CVC_MAX_BYTES reaching the end. Only its framing is
 * checked here.
 *
 * Next, the application authenticates the peer with
 * TC_PIV_SM_authenticate_response under the validated content signer, which
 * moves the session to READY, and calls TC_PIV_link_secure. session stays
 * bound to the link until TC_PIV_link_unsecure, TC_PIV_link_clear, a SELECT
 * of another application or a secure messaging failure, and must outlive
 * the link while bound. response holds TC_PIV_SM_KEY_RESPONSE_BYTES for the
 * largest answer.
 *
 * TC_PIV_ARGUMENT     NULL link, session, host_id, peer or workspace, a
 *                     cleared link, random without fill, a suite other than
 *                     CS2 and CS7, response with NULL data or below 2 bytes,
 *                     or overlap between response, *link, its scratch
 *                     buffers, *session, *workspace, *peer and host_id.
 * TC_PIV_UNSUPPORTED  the suite is disabled in this build, or the
 *                     application property template announced another suite.
 * TC_PIV_REFUSED      the PIV application is not selected, or the link is
 *                     secured or lost its session. Call TC_PIV_link_unsecure
 *                     before establishing a new session.
 * TC_PIV_ERROR        the random source failed, the transport failed or the
 *                     link is stopped.
 * TC_PIV_CARD_STATUS  the card answered other than 9000.
 * TC_PIV_INVALID      malformed key establishment framing.
 * TC_PIV_LIMIT        channel results.
 *
 * TC_PIV_OK: session is ESTABLISHING and bound. The link clears its VCI and
 * PIN status, since a new establishment ends the previous session (Part 2
 * 4.3). *peer borrows response. ARGUMENT, UNSUPPORTED and REFUSED change
 * nothing. Every other failure clears session, wipes workspace and response
 * and leaves *peer unchanged. */
TC_PIV_result TC_PIV_SM_key_request(TC_PIV_link* link, TC_PIV_SM* session, TC_PIV_SM_suite suite,
                                    const uint8_t host_id[8], TC_random_source random,
                                    TC_buffer response, TC_PIV_SM_peer* peer,
                                    TC_PIV_SM_workspace* workspace);

/* Protect GET DATA, VERIFY and GENERAL AUTHENTICATE with the bound session
 * (Part 2 4.2). Call after TC_PIV_SM_authenticate_response returned VALID
 * for that session. workspace and sm_scratch stay borrowed until the session
 * is unbound. sm_scratch holds the SM data field of one command and needs
 * TC_PIV_SM_COMMAND_DATA_BYTES(TC_PIV_COMMAND_MAX_NC) bytes. A larger command
 * needs TC_PIV_SM_COMMAND_DATA_BYTES of its data length. A command whose SM
 * data field exceeds the SM scratch, or whose fragments exceed the channel
 * scratch, the card limit or the exchange budget, returns TC_PIV_LIMIT before
 * it is protected, and the session stays READY. Plain data above
 * TC_PIV_SM_MAX_PLAIN_NC returns TC_PIV_ARGUMENT.
 *
 * Protected commands use CLA 0C, SHORT length fields and a new Le 00
 * (footnote 22). A data field above 255 bytes is sent as 1C fragments of
 * 255 bytes (4.2.4). SELECT and GET RESPONSE stay plain. The answer must be
 * [87 L 01 ciphertext] 99 02 SW 8E 08 MAC and nothing else (4.2.6). The
 * response MAC is checked before decryption, and the plaintext replaces the
 * ciphertext in the caller's response buffer. The status inside 99 becomes
 * the command status. Inner statuses other than 9000 keep the session.
 *
 * Session loss (4.3, footnote 25). After the command is protected, every
 * other outcome clears the session, clears the VCI and PIN status, wipes the
 * response and sets sm_lost:
 * TC_PIV_CARD_STATUS  an outer status other than 9000, such as 6882, 6982,
 *                     6987, 6988, 6CXX or a chained fragment answered 6883.
 *                     TC_PIV_link_status returns the outer status.
 * TC_PIV_INVALID      malformed SM DOs, 87 on a VERIFY answer, a failed
 *                     response MAC or padding. TC_PIV_link_status returns 0.
 * TC_PIV_LIMIT        the response capacity or the exchange budget ran out
 *                     during the exchange.
 * TC_PIV_ERROR        the transport failed, or TC_PIV_SM_protect failed, for
 *                     example on an exhausted counter.
 * A transport failure on a plain command, such as SELECT, also ends a bound
 * session this way, since the stopped link cannot continue it.
 * While sm_lost, those commands return TC_PIV_REFUSED until
 * TC_PIV_link_unsecure.
 *
 * TC_PIV_ARGUMENT  NULL link or workspace, a cleared link, no bound session,
 *                  a session outside READY, a secured link, sm_scratch with
 *                  NULL data or below its minimum, or overlap between
 *                  sm_scratch, *workspace, *link, its command scratch and the
 *                  session.
 *
 * *link changes only on TC_PIV_OK. */
TC_PIV_result TC_PIV_link_secure(TC_PIV_link* link, TC_PIV_SM_workspace* workspace,
                                 TC_buffer sm_scratch);

/* 1 when the secured live link is bound to the exact authenticated card CVC,
 * else 0. The comparison is constant-time over a SHA-256 binding. */
int TC_PIV_link_sm_peer_matches(const TC_PIV_link* link, TC_bytes certificate);

/* Clear the bound session, wipe the secure messaging scratch and clear the
 * secured, sm_lost and VCI state, so the next commands travel in plaintext.
 * Accepts NULL and an unsecured link. */
void TC_PIV_link_unsecure(TC_PIV_link* link);
#endif

#ifdef __cplusplus
}
#endif
#endif
