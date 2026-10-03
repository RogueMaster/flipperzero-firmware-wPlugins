/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Link state and the command dispatch shared by the PIV command sources. */
#ifndef TC_PIV_LINK_INTERNAL_H_
#define TC_PIV_LINK_INTERNAL_H_
#include <tiny_crypto/piv_command.h>
#include "piv_aid_internal.h"

/* TC_PIV_link.flags bits. */
enum {
  TC_PIV_LINK_SECURED = 1u << 0,
  TC_PIV_LINK_SM_LOST = 1u << 1,
  TC_PIV_LINK_VCI = 1u << 2,
  TC_PIV_LINK_PIN_VERIFIED = 1u << 3
};

enum {
  TC_PIV_SW_SUCCESS_VALUE = 0x9000,
  TC_PIV_SW_END_OF_OBJECT_VALUE = 0x6282,
  TC_PIV_PLAIN_CLA = 0x00
};

/* Instructions that secure messaging protects (SP 800-73-5 Part 2 4.2).
 * CHANGE REFERENCE DATA (24) is outside this library. */
enum {
  TC_PIV_INS_VERIFY = 0x20,
  TC_PIV_INS_GENERAL_AUTHENTICATE = 0x87,
  TC_PIV_INS_GET_DATA = 0xcb
};

/* Secure messaging operations installed by piv_sm_apdu.c when a session is
 * bound, so images without secure messaging carry none of its code.
 * transceive sends one protected command and returns its authenticated
 * answer. unbind clears the session, wipes the secure messaging scratch and
 * leaves the link unsecured. */
struct tc_piv_link_security {
  TC_PIV_result (*transceive)(TC_PIV_link* link, const TC_APDU_command* command, TC_buffer response,
                              TC_APDU_response* out);
  void (*unbind)(TC_PIV_link* link);
};

/* Prefix for application PIV or TWIC, NULL for another value. */
const uint8_t* tc_piv_aid_prefix(TC_PIV_application_id application);

/* 1 when link holds an initialized channel. */
int tc_piv_link_ready(const TC_PIV_link* link);
/* 1 when length bytes at data are disjoint from *link, its command scratch,
 * which the channel wipes after every transmit, and a bound secure messaging
 * session, workspace and scratch. */
int tc_piv_link_disjoint(const TC_PIV_link* link, const void* data, size_t length);
/* 1 when response can receive an answer for link and out: data present, at
 * least SW1 SW2, disjoint from *link, its command scratch and out_length bytes
 * at out. */
int tc_piv_response_valid(const TC_PIV_link* link, TC_buffer response, const void* out,
                          size_t out_length);

/* Send command for the command kind and collect the answer in response.
 * Records the kind, and the final SW as the link status on TC_PIV_OK. Every
 * other result sets the link status to 0, except a secure messaging error
 * status, and wipes response. TC_PIV_OK means the card answered with any
 * status. TC_PIV_ERROR also ends a bound session. On a secured link GET DATA, VERIFY and GENERAL AUTHENTICATE go
 * through the installed security operations, and the answer is the decrypted
 * inner response. A link that lost its session refuses them. The callers
 * checked their arguments. */
TC_PIV_result tc_piv_link_transceive(TC_PIV_link* link, TC_PIV_command kind,
                                     const TC_APDU_command* command, TC_buffer response,
                                     TC_APDU_response* out);

/* Clear a bound secure messaging session and leave the link unsecured. */
void tc_piv_link_unbind(TC_PIV_link* link);
/* End a bound session after a secure messaging or transport failure: unbind,
 * clear the VCI and PIN status and set sm_lost, so protected commands are
 * refused until TC_PIV_link_unsecure (Part 2 4.3, footnote 25). */
void tc_piv_link_session_lost(TC_PIV_link* link);

/* 1 when digits holds minimum to 8 ASCII digits 30 to 39 (SP 800-73-5 Part 2
 * 2.4.3). */
int tc_piv_digits_valid(TC_bytes digits, size_t minimum);
/* Send VERIFY P1 00 for reference with digits padded with FF to 8 bytes
 * (Part 2 2.4.3, 3.2.1). The padded copy lives in a stack array that is
 * wiped. The caller checked digits with tc_piv_digits_valid and the
 * reference rules. On TC_PIV_OK *sw holds the card status, and VERIFY
 * answers with data are INVALID. */
TC_PIV_result tc_piv_verify_submit(TC_PIV_link* link, uint8_t reference, TC_bytes digits,
                                   uint16_t* sw);

/* The APDU and PIV results share their first six values and meanings. */
TC_PIV_result tc_piv_channel_result(TC_APDU_result result);

/* End a command that failed after transmit: wipe response, set the link
 * status to status and return result. */
TC_PIV_result tc_piv_link_fail(TC_PIV_link* link, TC_buffer response, uint16_t status,
                               TC_PIV_result result);
#endif
