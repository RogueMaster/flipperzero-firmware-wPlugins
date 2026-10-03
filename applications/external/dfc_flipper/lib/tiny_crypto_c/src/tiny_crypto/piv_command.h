/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC card commands over an APDU channel: SELECT with the
 * application property template, GET DATA with object framing, VERIFY
 * (retry query and PIN), per-command status meanings and link state.
 * Standards: NIST SP 800-73-5 Part 1 and Part 2, SP 800-73-4 (TWIC Legacy
 * PIV applications), TWIC Part 2 v5 sections 3.3.6, 4.1 and 5, TWIC Part 3 v4
 * Appendix D.3, ISO/IEC 7816-4:2020 12.8.1.
 * Configuration: TC_ENABLE_PIV_COMMAND (requires TC_ENABLE_APDU and
 * TC_ENABLE_TLV).
 * Limitations: secure messaging is in piv_sm_apdu.h. CHANGE REFERENCE DATA, RESET
 * RETRY COUNTER, PUT DATA, GENERATE ASYMMETRIC KEY PAIR and OCC VERIFY (96,
 * 97) are outside this module. The library owns no I/O, reader selection or
 * PIN entry.
 * Contracts: docs/api.md.
 * Guide: docs/piv-card.md. */
#ifndef TINY_CRYPTO_PIV_COMMAND_H_
#define TINY_CRYPTO_PIV_COMMAND_H_
#include <tiny_crypto/apdu.h>
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/tlv.h>
#if TC_ENABLE_PIV_KEY_PROOF
#include <tiny_crypto/key_challenge.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Result of every PIV card-interaction function. The first six values follow
 * the TC_APDU_result order and meaning. INVALID is a malformed card answer.
 * LIMIT is a short response buffer, scratch, card size limit or exhausted
 * exchange budget. ERROR is a transport failure or a stopped link.
 * CARD_STATUS means the card completed the command with a status other than
 * success. TC_PIV_link_status returns that status and TC_PIV_status_classify
 * gives its meaning. REFUSED means a safety or state precondition failed
 * before the command, and nothing was sent. On a secured link GET DATA,
 * VERIFY and GENERAL AUTHENTICATE also return the session-loss results of
 * piv_sm_apdu.h. */
typedef TC_result TC_PIV_result;
#define TC_PIV_OK TC_RESULT_OK
#define TC_PIV_INVALID TC_RESULT_INVALID
#define TC_PIV_LIMIT TC_RESULT_LIMIT
#define TC_PIV_ARGUMENT TC_RESULT_ARGUMENT
#define TC_PIV_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_PIV_ERROR TC_RESULT_ERROR
#define TC_PIV_CARD_STATUS TC_RESULT_CARD_STATUS
#define TC_PIV_REFUSED TC_RESULT_REFUSED

/* Card interface of the link. The application states it, since the PIN and
 * pairing-code rules depend on it (SP 800-73-5 Part 1 Table 4). */
typedef enum { TC_PIV_CONTACT, TC_PIV_CONTACTLESS } TC_PIV_interface;

/* Card application. NONE means no application is selected on the link. */
typedef enum {
  TC_PIV_APPLICATION_NONE,
  TC_PIV_APPLICATION_PIV,
  TC_PIV_APPLICATION_TWIC
} TC_PIV_application_id;

/* Command kinds for TC_PIV_status_classify. */
typedef enum {
  TC_PIV_COMMAND_SELECT,
  TC_PIV_COMMAND_GET_DATA,
  TC_PIV_COMMAND_VERIFY,
  TC_PIV_COMMAND_GENERAL_AUTHENTICATE
} TC_PIV_command;

/* Data field of secure messaging key establishment, 7C {81 {CB_H, ID_sH,
 * Q_eH}, 82 00}: 80 bytes with a P-256 point (CS2) and 112 with a P-384
 * point (CS7) (SP 800-73-5 Part 2 4.1.8). 0 without TC_ENABLE_PIV_SM_APDU. */
#if TC_ENABLE_PIV_SM_APDU && TC_PIV_SM_ENABLE_CS7
#define TC_PIV_SM_KEY_REQUEST_BYTES 112u
#elif TC_ENABLE_PIV_SM_APDU
#define TC_PIV_SM_KEY_REQUEST_BYTES 80u
#else
#define TC_PIV_SM_KEY_REQUEST_BYTES 0u
#endif
/* Largest command data field that a link may protect with secure messaging.
 * SELECT sends 11, GET DATA 5 and VERIFY 8 bytes. With TC_ENABLE_PIV_KEY_PROOF
 * a key proof sends a 7C template of up to TC_KEY_CHALLENGE_MAX_INPUT_BYTES +
 * 12 bytes (piv_key_proof.h). */
#if TC_ENABLE_PIV_KEY_PROOF
#define TC_PIV_COMMAND_MAX_NC ((size_t)TC_KEY_CHALLENGE_MAX_INPUT_BYTES + 12u)
#else
#define TC_PIV_COMMAND_MAX_NC 32u
#endif
/* Command scratch of an EXTENDED link: one encoded command of the larger of
 * TC_PIV_COMMAND_MAX_NC and the plain key establishment request. A SHORT link
 * needs TC_APDU_SHORT_COMMAND_MAX_BYTES, which covers both. */
#define TC_PIV_EXTENDED_SCRATCH_BYTES                                                              \
  TC_APDU_EXTENDED_COMMAND_BYTES((size_t)TC_PIV_COMMAND_MAX_NC > TC_PIV_SM_KEY_REQUEST_BYTES       \
                                     ? (size_t)TC_PIV_COMMAND_MAX_NC                               \
                                     : TC_PIV_SM_KEY_REQUEST_BYTES)
/* Response buffer bytes for nr plain data bytes on any link, plain or secure
 * messaging: nr padded to whole AES blocks, the 87 header (5), 99 04, 8E 0A
 * and SW1 SW2 (SP 800-73-5 Part 2 4.2.5). Valid while the sum fits size_t. */
#define TC_PIV_PADDED_BYTES(n) ((((size_t)(n)) / 16u + 1u) * 16u)
#define TC_PIV_RESPONSE_BYTES(nr)                                                                  \
  (TC_PIV_PADDED_BYTES(nr) + 5u + 4u + 10u + (size_t)TC_APDU_STATUS_BYTES)

/* Link configuration.
 * channel      APDU channel options: format, exchange budget and card limits.
 *              TC_PIV_select replaces the GET RESPONSE flags.
 * interface    the interface the card is reached over.
 * response_ne  Ne for data-returning commands. 0 selects 256 (Le 00, SP
 *              800-73-5 Part 2 3.1.2). SHORT allows 1 to 256, EXTENDED 1 to
 *              65536. The channel lowers it to the card's DO 7F66 response
 *              limit minus 2. */
typedef struct {
  TC_APDU_channel_options channel;
  TC_PIV_interface interface;
  uint32_t response_ne;
} TC_PIV_link_options;

/* Card session over one transport. Members are private. Do not copy an
 * initialized link. The link borrows the command scratch buffer and the
 * transport context until TC_PIV_link_clear. With TC_ENABLE_PIV_SM_APDU it
 * also borrows a bound secure messaging session, its workspace and scratch
 * (piv_sm_apdu.h). */
struct tc_piv_link_security;
typedef struct {
  TC_APDU_channel channel;
  const struct tc_piv_link_security* security;
  void* sm;
  void* sm_workspace;
  uint8_t* sm_scratch;
  size_t sm_scratch_capacity;
  uint32_t response_ne;
  uint16_t status;
  uint8_t interface, application, profile, command, sm_suite, flags;
} TC_PIV_link;

/* Link state for access decisions. secured, sm_lost, vci and pin_verified
 * are 0 or 1. sm_suite is the secure messaging suite (0x27, 0x2E) the
 * selected application announced in its property template, or 0. */
typedef struct {
  TC_PIV_interface interface;
  TC_PIV_application_id application;
  TC_PIV_card_profile profile;
  uint8_t secured, sm_lost, vci, pin_verified, sm_suite;
} TC_PIV_link_info;

#if TC_ENABLE_PIV_COMMAND
/* Start a link. command_scratch holds one encoded command fragment and is
 * wiped after every transmit. SHORT needs TC_APDU_SHORT_COMMAND_MAX_BYTES.
 * EXTENDED needs TC_PIV_EXTENDED_SCRATCH_BYTES, or the channel's
 * max_command_bytes when smaller. No application is selected until
 * TC_PIV_select succeeds.
 *
 * TC_PIV_ARGUMENT  NULL link or options, an unknown interface, response_ne
 *                  outside the format, scratch below its minimum or
 *                  overlapping *link or *options, or channel options that
 *                  TC_APDU_channel_init rejects.
 *
 * *link changes only on TC_PIV_OK. */
TC_PIV_result TC_PIV_link_init(TC_PIV_link* link, TC_APDU_transport transport,
                               const TC_PIV_link_options* options, TC_buffer command_scratch);

/* Copy the link state to *out. Does nothing when either pointer is NULL. */
void TC_PIV_link_info_get(const TC_PIV_link* link, TC_PIV_link_info* out);

/* Status word of the last command the card completed, as returned with
 * TC_PIV_OK or TC_PIV_CARD_STATUS. Under secure messaging it is the
 * authenticated status inside DO 99, or the outer status when the card
 * reported a secure messaging error (piv_sm_apdu.h). After a PIN refusal for
 * the retry floor it holds the answer to the retry query. 0 before any
 * command, after a malformed or unauthenticated answer, a transport failure or
 * LIMIT, and for a NULL link. A call rejected or refused before sending
 * leaves it unchanged. */
uint16_t TC_PIV_link_status(const TC_PIV_link* link);

/* Wipe the scratch buffer and the link state, and clear a bound secure
 * messaging session and its scratch. Accepts NULL. */
void TC_PIV_link_clear(TC_PIV_link* link);
#endif

/* Accept a TWIC application version 01 with a sub-version other than 01
 * (Legacy) and 03 (NEXGEN) as TC_TWIC_LEGACY_CARD. TWIC Part 3 v4 Appendix
 * D.3 states that version 01 is backward compatible with the Legacy data
 * model and leaves the decision to the reader. */
enum { TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE = 1u << 0 };

/* Application property template of a selected application.
 * aid         the complete AID from 4F: 9-byte prefix and 2 version bytes.
 * label, url  50 and 5F50 values, or empty.
 * algorithms  the AC value (80 algorithm identifiers and 06 01 00), or empty.
 * max_*       DO 7F66 limits (ISO/IEC 7816-4 12.8.1), 0 when absent. Values
 *             above SIZE_MAX read as SIZE_MAX.
 * profile     TC_PIV_CARD, TC_TWIC_LEGACY_CARD or TC_TWIC_NEXGEN_CARD.
 * version     the last two AID bytes.
 * sm_suite    0x27 or 0x2E when AC lists that suite, else 0.
 * Spans borrow the response. */
typedef struct {
  TC_bytes aid, label, url, algorithms;
  size_t max_command_bytes, max_response_bytes;
  TC_PIV_card_profile profile;
  uint8_t version[2];
  uint8_t sm_suite;
} TC_PIV_application;

#if TC_ENABLE_PIV_COMMAND
/* Read a SELECT response data field for the expected application (SP
 * 800-73-5 Part 2 3.1.1 Tables 3 to 5, TWIC Part 2 v5 5.1.1). Framing uses the
 * ISO/IEC 7816-4 profile with at most 4096 bytes, 64 elements and depth 4.
 *
 * The first top-level DO is the only 61 template. 7F66 may follow it once
 * and holds exactly two positive 02 integers, at least 4 and 3 bytes. Other
 * top-level DOs are skipped. Inside 61: exactly one 4F with the expected
 * 9-byte AID prefix and 2 version bytes, one 79 holding one nonempty 4F, and
 * at most one 50, 5F50 and AC. In AC every 80 holds one byte, at least one 80
 * and exactly one 06 01 00 are present, and at most one of 27 and 2E is
 * listed (Table 5). Unknown DOs inside the templates are skipped.
 *
 * Profiles: PIV version 01 00 is TC_PIV_CARD. TWIC version 01 01 is Legacy
 * and 01 03 is NEXGEN (TWIC Part 2 v5 4.1). Another TWIC sub-version is
 * UNSUPPORTED unless flags has TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE.
 *
 * TC_TLV_ARGUMENT     NULL out, response with NULL data and a nonzero
 *                     length, an unknown application or flag, or out
 *                     overlapping response.
 * TC_TLV_INVALID      malformed framing, a missing or duplicate DO, a
 *                     foreign AID or a malformed AC or 7F66.
 * TC_TLV_LIMIT        a framing limit above.
 * TC_TLV_UNSUPPORTED  another application version.
 *
 * out changes only on TC_TLV_OK. */
TC_TLV_result TC_PIV_application_read(TC_bytes response, TC_PIV_application_id expected,
                                      unsigned flags, TC_PIV_application* out);

/* SELECT the application by AID (SP 800-73-5 Part 2 3.1.1, TWIC Part 2 v5
 * 5.1): the complete PIV AID, or the 9-byte TWIC AID prefix, with Le 00. The
 * command is always plain. Selecting another application sets the card's
 * security statuses to FALSE, and reselecting the PIV application keeps them
 * (Part 2 3.1.1). The link clears its VCI and PIN status in both cases, so
 * query the PIN again with TC_PIV_verify_status. Selecting an application
 * other than the selected one also clears a bound secure messaging session.
 * On success the link records
 * the application and profile, applies the DO 7F66 limits to the channel and
 * sets TC_APDU_GET_RESPONSE_PLAIN_CLA (Part 2 4.2.6, A.4.1, TWIC Part 2 v5
 * Appendix E). GET RESPONSE after 61 00 requests 256 bytes with Le 00 on both
 * applications (TWIC Part 2 v5 Appendix E). Any other outcome after transmit
 * leaves no application selected.
 *
 * TC_PIV_ARGUMENT     NULL link or out, a cleared link, an unknown
 *                     application or flag, response with NULL data or below
 *                     2 bytes, or response overlapping *link, its scratch
 *                     buffers or *out.
 * TC_PIV_CARD_STATUS  the card answered other than 9000, such as 6A82.
 * TC_PIV_INVALID, TC_PIV_LIMIT, TC_PIV_UNSUPPORTED
 *                     TC_PIV_application_read results, or channel results.
 * TC_PIV_ERROR        the transport failed or the link is stopped.
 *
 * out borrows response and changes only on TC_PIV_OK. Every failure after
 * the argument checks wipes response. */
TC_PIV_result TC_PIV_select(TC_PIV_link* link, TC_PIV_application_id application, unsigned flags,
                            TC_buffer response, TC_PIV_application* out);
#endif

/* Form of a GET DATA answer. CONTAINER is 53, TEMPLATE is a DO with the
 * requested tag, NONE is a TWIC 9000 without data. */
typedef enum { TC_PIV_FORM_CONTAINER, TC_PIV_FORM_TEMPLATE, TC_PIV_FORM_NONE } TC_PIV_object_form;

/* One data object read with GET DATA. encoded is the complete returned TLV
 * and value its value, both borrowed from the response buffer. An empty
 * object has an empty value. FORM_NONE has both spans empty. status is 9000
 * or 6282. form is a TC_PIV_object_form value. */
typedef struct {
  TC_bytes encoded, value;
  uint16_t status;
  uint8_t form;
} TC_PIV_data_object;

#if TC_ENABLE_PIV_COMMAND
/* GET DATA for one tag of 1 to 3 bytes (SP 800-73-5 Part 2 3.1.2): CLA 00,
 * INS CB, P1 P2 3F FF, data 5C L tag and Ne = response_ne. A secured link
 * sends it under secure messaging and frames the decrypted answer, which
 * stays in the response buffer (piv_sm_apdu.h). The answer to 9000
 * or 6282 (ISO/IEC 7816-4 Table 7, TWIC Part 2 v5 5.2) must be exactly one
 * TLV spanning the data field.
 *
 * PIV application: 7E and 7F61 answer with their own tag and every other tag
 * with 53 (Part 2 3.1.2). The only empty form is 53 00 (Part 1 4.1.1).
 * TWIC application: 53 or the requested tag. 53 00, TAG 00 and, for a
 * constructed tag, TAG 02 80 00 are empty objects (TWIC Part 2 v5 3.3.6). A
 * 9000 without data is FORM_NONE (TWIC Part 2 v5 4.5).
 *
 * TC_PIV_ARGUMENT     NULL link or out, a cleared link, a tag other than one
 *                     complete ISO/IEC 7816-4 tag of 1 to 3 bytes, response
 *                     with NULL data or below 2 bytes, or response overlapping
 *                     *link, its scratch buffers, *out or the tag.
 * TC_PIV_REFUSED      no application is selected, or the link lost its
 *                     secure messaging session.
 * TC_PIV_CARD_STATUS  another status, such as 6982, 6A81, 6A82 or 6A88.
 * TC_PIV_INVALID      framing other than above, or trailing bytes.
 * TC_PIV_LIMIT, TC_PIV_ERROR
 *                     channel results.
 *
 * out borrows response and changes only on TC_PIV_OK. Every failure after
 * the argument checks wipes response. */
TC_PIV_result TC_PIV_get_data(TC_PIV_link* link, TC_bytes tag, TC_buffer response,
                              TC_PIV_data_object* out);
#endif

/* Verification state of one key reference. verified is 1 when the security
 * status is TRUE. retries holds X of 63CX when retries_known is 1. submitted
 * is 1 when TC_PIV_pin_verify sent the PIN. */
typedef struct {
  unsigned retries;
  uint8_t verified, submitted, retries_known;
} TC_PIV_reference_status;

#if TC_ENABLE_PIV_COMMAND
/* VERIFY without data (SP 800-73-5 Part 2 3.2.1): P1 00, no Lc, no Le. It
 * reports whether reference is verified or how many retries remain.
 * reference is 80 (PIV PIN), 00 (Global PIN) or 98 (pairing code). 9000 sets
 * verified, and for 80 and 00 marks the PIN verified on the link. Any other
 * answer for 80 or 00 clears that mark, since the reference is then FALSE on
 * the card (Part 2 3.2.1.1). 63CX sets retries. 6300 reports neither (Part 2
 * footnote 7).
 *
 * TC_PIV_ARGUMENT     NULL link or out, a cleared link, or another reference.
 * TC_PIV_UNSUPPORTED  OCC references 96 and 97, or the TWIC application.
 * TC_PIV_REFUSED      no application is selected, reference 80 or 00 on a
 *                     contactless link without the VCI (Part 2 3.2.1), 98
 *                     on a contactless link without secure messaging, or a
 *                     link that lost its secure messaging session.
 * TC_PIV_CARD_STATUS  another status, such as 6983 (blocked, or the
 *                     contactless intermediate retry value) or 6A88.
 * TC_PIV_INVALID, TC_PIV_LIMIT, TC_PIV_ERROR
 *                     channel results.
 *
 * out changes only on TC_PIV_OK. */
TC_PIV_result TC_PIV_verify_status(TC_PIV_link* link, uint8_t reference,
                                   TC_PIV_reference_status* out);

/* Verify the PIV PIN (80) or Global PIN (00) (SP 800-73-5 Part 2 2.4.3,
 * 3.2.1.1). pin holds 6 to 8 ASCII digits. The call first queries the retry
 * counter. A reference already verified returns TC_PIV_OK with submitted 0.
 * Otherwise the PIN is sent once, padded with FF to 8 bytes, only when the
 * query reported at least minimum_retries tries, so the last tries stay
 * unspent. The padded PIN lives in a stack array and the channel scratch,
 * and on a secured link in the secure messaging scratch, and all are wiped. On success the link marks the PIN verified. A rejected
 * submission clears that mark.
 *
 * TC_PIV_ARGUMENT     NULL link or out, a cleared link, another reference,
 *                     pin with NULL data, a length outside 6 to 8, a
 *                     non-digit, or overlapping *link, its scratch buffers or
 *                     *out, or minimum_retries outside 2 to 15.
 * TC_PIV_UNSUPPORTED  the TWIC application.
 * TC_PIV_REFUSED      no application is selected, a contactless link
 *                     without the VCI (Part 1 Table 4), a link that lost its
 *                     secure messaging session, or a query reporting
 *                     fewer than minimum_retries tries or no count. The PIN
 *                     never reached the card.
 * TC_PIV_CARD_STATUS  the query or the submission answered another status,
 *                     such as 63CX (wrong PIN), 6983 or 6A80.
 * TC_PIV_INVALID, TC_PIV_LIMIT, TC_PIV_ERROR
 *                     channel results. The PIN may have reached the card when
 *                     the submission failed this way.
 *
 * out changes only on TC_PIV_OK. */
TC_PIV_result TC_PIV_pin_verify(TC_PIV_link* link, uint8_t reference, TC_bytes pin,
                                unsigned minimum_retries, TC_PIV_reference_status* out);
#endif

/* Meanings of PIV and TWIC status words (SP 800-73-5 Part 1 5.6 Table 7). */
typedef enum {
  TC_PIV_SW_SUCCESS,             /* 9000 */
  TC_PIV_SW_END_OF_OBJECT,       /* 6282 on GET DATA */
  TC_PIV_SW_VERIFY_FAILED,       /* 6300 or 63CX on VERIFY */
  TC_PIV_SW_SM_UNSUPPORTED,      /* 6882 */
  TC_PIV_SW_SECURITY_STATUS,     /* 6982 */
  TC_PIV_SW_BLOCKED,             /* 6983 */
  TC_PIV_SW_SM_MISSING,          /* 6987 */
  TC_PIV_SW_SM_INCORRECT,        /* 6988 */
  TC_PIV_SW_WRONG_DATA,          /* 6A80 */
  TC_PIV_SW_NOT_SUPPORTED,       /* 6A81 */
  TC_PIV_SW_NOT_FOUND,           /* 6A82, and 6A88 on TWIC GET DATA */
  TC_PIV_SW_NO_MEMORY,           /* 6A84 */
  TC_PIV_SW_WRONG_P1P2,          /* 6A86 */
  TC_PIV_SW_REFERENCE_NOT_FOUND, /* 6A88 */
  TC_PIV_SW_OTHER
} TC_PIV_status;

#if TC_ENABLE_PIV_COMMAND
/* Classify sw as the answer to command on application. 6A88 is
 * REFERENCE_NOT_FOUND except on TWIC GET DATA, where it means the object is
 * absent (TWIC Part 2 v5 5.2). 63CX on VERIFY writes X to *retries when
 * retries is non-NULL. 6300 leaves *retries unchanged, since the pairing
 * code has no counter (Part 2 footnote 7). */
TC_PIV_status TC_PIV_status_classify(uint16_t sw, TC_PIV_command command,
                                     TC_PIV_application_id application, unsigned* retries);
#endif

#ifdef __cplusplus
}
#endif
#endif
