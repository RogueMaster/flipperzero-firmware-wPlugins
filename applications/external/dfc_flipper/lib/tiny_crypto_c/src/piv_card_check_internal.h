/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * State shared by the card check phases: the request, the workspace, the
 * report being written and the first abort. Each phase appends entries in
 * report order and reads the entries of earlier phases for its
 * prerequisites. */
#ifndef TC_PIV_CARD_CHECK_INTERNAL_H_
#define TC_PIV_CARD_CHECK_INTERNAL_H_
#include <tiny_crypto/piv_card_check.h>

typedef struct {
  const TC_PIV_card_check_request* request;
  TC_PIV_card_check_workspace* workspace;
  size_t* work;
  TC_PIV_card_report* report;
  /* Bytes of workspace->certificates that hold decoded certificates. */
  size_t certificate_used;
  /* OK, or the LIMIT or ERROR that aborts the check. */
  TC_PIV_result result;
  /* 1 for a TWIC credential on the PIV application. */
  uint8_t twic_piv;
} tc_piv_check_run;

/* An entry of kind for container and key with outcome PASSED and status
 * VALID, to be adjusted before tc_piv_check_add. */
TC_PIV_check tc_piv_check_make(uint8_t kind, uint16_t container, uint8_t key_reference);

/* Append check. A full report sets run->result to LIMIT. Returns the
 * appended entry, or NULL once the run has aborted. */
const TC_PIV_check* tc_piv_check_add(tc_piv_check_run* run, const TC_PIV_check* check);

/* Set outcome and reason from a validator status: VALID passes, INVALID and
 * REVOKED fail, UNAVAILABLE is NO_EVIDENCE, UNSUPPORTED and LIMIT keep their
 * name. ERROR aborts the run and returns 0, else 1. */
int tc_piv_check_status(tc_piv_check_run* run, TC_PIV_check* check, TC_credential_status status);

/* The same for a TLV reader result: OK passes, LIMIT and UNSUPPORTED keep
 * their name, ARGUMENT aborts, every other result fails. */
int tc_piv_check_tlv(tc_piv_check_run* run, TC_PIV_check* check, TC_TLV_result result);

/* Set outcome and reason for an object that was not read in full: ABSENT,
 * DENIED and EMPTY fail when missing_fails, else they are NOT_CHECKABLE.
 * RESTRICTED, OVERSIZED and SKIPPED are NOT_CHECKABLE. A contactless denial
 * on the PIV application of a TWIC card is NOT_CHECKABLE (TWIC Part 2 v5
 * 4.2). The card status is recorded. */
void tc_piv_check_unread(const tc_piv_check_run* run, TC_PIV_check* check,
                         const TC_PIV_object* object, int missing_fails);

/* NOT_CHECKABLE with reason. */
void tc_piv_check_not_checkable(TC_PIV_check* check, uint8_t reason);

/* 1 when the report holds a PASSED entry of kind for container. */
int tc_piv_check_passed(const TC_PIV_card_report* report, uint8_t kind, uint16_t container);

/* The first inventory entry of kind, or of kind with key_reference when it
 * is nonzero, or NULL. */
const TC_PIV_object* tc_piv_check_object(const TC_PIV_inventory* inventory, uint8_t kind,
                                         uint8_t key_reference);

/* The credential profile is PIV. */
static inline int tc_piv_check_piv(const tc_piv_check_run* run)
{
  return run->report->profile == TC_PIV_CARD;
}

/* Phases in report order. Each returns early once run->result is not OK. */
void tc_piv_check_card_certificate(tc_piv_check_run* run);
void tc_piv_check_chuid(tc_piv_check_run* run);
void tc_piv_check_key_certificates(tc_piv_check_run* run);
void tc_piv_check_security(tc_piv_check_run* run);
void tc_piv_check_biometrics(tc_piv_check_run* run);
void tc_piv_check_printed(tc_piv_check_run* run);
void tc_piv_check_discovery(tc_piv_check_run* run);
void tc_piv_check_secure_messaging(tc_piv_check_run* run);

/* The card context purpose is empty or names card authentication for
 * profile. */
int tc_piv_check_card_purpose_valid(const TC_validation_options* options,
                                    TC_PIV_card_profile profile);

/* The value of the BC element of a biometric container value (SP 800-73-5
 * Part 1 Tables 13 and 14). */
TC_TLV_result tc_piv_check_biometric_value(TC_bytes value, const TC_TLV_limits* limits,
                                           TC_bytes* out);
/* Decode the certificate container object with profile into the next free
 * workspace certificate bytes and parse it with the context's parsing
 * limits. Returns the TLV result. */
TC_TLV_result tc_piv_check_certificate_decode(tc_piv_check_run* run, const TC_PIV_object* object,
                                              TC_PIV_certificate_profile profile,
                                              const TC_validation_context* context,
                                              TC_PIV_certificate* container,
                                              TC_X509_certificate* out);
#endif
