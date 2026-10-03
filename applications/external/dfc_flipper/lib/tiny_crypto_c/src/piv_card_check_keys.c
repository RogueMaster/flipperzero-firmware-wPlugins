/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Card keys: the secure messaging Certificate Signer and card CVC checks,
 * and live key proofs appended to a report. */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "internal.h"
#include "piv_card_check_internal.h"
#include <tiny_crypto/piv_sm_apdu.h>
#include "credential_status_internal.h"
#if TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_cvc.h>
#endif
#include <string.h>

enum { SUITE_CS2 = 0x27, SUITE_CS7 = 0x2e };

/* SM_CVC: the card CVC of the session verifies under the validated signer
 * with the 5FC122 intermediate and the CHUID GUID (SP 800-73-5 Part 2
 * section 4.1.5), on a link that is secured now. */
static void card_cvc_check(tc_piv_check_run* run, const TC_PIV_certificate* fields,
                           const TC_X509_certificate* signer, TC_PIV_check* check)
{
  const TC_PIV_card_check_request* request = run->request;
  if (!request->sm_card_cvc.length) {
    tc_piv_check_not_checkable(check, TC_PIV_REASON_NOT_REQUESTED);
    return;
  }
  if (!signer || !run->report->has_chuid) {
    tc_piv_check_not_checkable(check, TC_PIV_REASON_DEPENDENCY);
    return;
  }
  TC_PIV_link_info info = {0};
  if (request->link)
    TC_PIV_link_info_get(request->link, &info);
  if (!request->link || !info.secured || info.sm_lost) {
    tc_piv_check_not_checkable(check, TC_PIV_REASON_RESTRICTED);
    return;
  }
#if TC_ENABLE_PIV_CVC
  if (!TC_PIV_link_sm_peer_matches(request->link, request->sm_card_cvc)) {
    tc_piv_check_status(run, check, TC_CREDENTIAL_INVALID);
    return;
  }
  if (info.sm_suite != SUITE_CS2 && info.sm_suite != SUITE_CS7) {
    tc_piv_check_not_checkable(check, TC_PIV_REASON_UNSUPPORTED);
    return;
  }
  const TC_validation_options* options = request->content->options;
  const TC_PIV_CVC_chain_request chain = {
      request->sm_card_cvc, fields->intermediate_cvc, run->report->chuid.object.card_uuid,
      info.sm_suite == SUITE_CS2 ? TC_EC_P256 : TC_EC_P384, signer};
  TC_PIV_CVC verified;
  tc_piv_check_status(run, check,
                      tc_credential_signature_status(
                          TC_PIV_CVC_chain_verify(&chain, &options->parsing, &options->signatures,
                                                  &run->workspace->point, run->work, &verified)));
#else
  (void)fields;
  tc_piv_check_not_checkable(check, TC_PIV_REASON_UNSUPPORTED);
#endif
}

/* SM_SIGNER, REVOCATION 1017 and SM_CVC. The Certificate Signer of 5FC122
 * validates as a content signer (SP 800-73-5 Part 1 section 3.3.7). */
void tc_piv_check_secure_messaging(tc_piv_check_run* run)
{
  if (run->result != TC_PIV_OK)
    return;
  const TC_PIV_object* object =
      tc_piv_check_object(run->request->inventory, TC_PIV_KIND_SM_SIGNER, 0);
  if (!object)
    return;
  const uint16_t container = object->info->container;
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_SM_SIGNER, container, 0);
  TC_PIV_check revocation = tc_piv_check_make(TC_PIV_CHECK_REVOCATION, container, 0);
  TC_PIV_check cvc = tc_piv_check_make(TC_PIV_CHECK_SM_CVC, container, 0);
  TC_PIV_certificate fields;
  TC_X509_validation_report validated;
  const TC_X509_certificate* signer = NULL;
  memset(&fields, 0, sizeof fields);
  if (object->state != TC_PIV_OBJECT_PRESENT) {
    tc_piv_check_unread(run, &check, object, 0);
    tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
  } else {
    TC_X509_certificate parsed;
    const TC_TLV_result decoded = tc_piv_check_certificate_decode(
        run, object, TC_PIV_CERTIFICATE_SM_SIGNER, run->request->content, &fields, &parsed);
    if (decoded != TC_TLV_OK) {
      if (!tc_piv_check_tlv(run, &check, decoded))
        return;
      tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
    } else {
      const TC_credential_status status = TC_PIV_content_signer_validate(
          fields.certificate, run->report->profile, run->request->content, run->work, &validated);
      if (!tc_piv_check_status(run, &check, status))
        return;
      if (status == TC_CREDENTIAL_VALID) {
        signer = &validated.certificate;
        tc_piv_check_status(run, &revocation,
                            validated.revocation_checked ? TC_CREDENTIAL_VALID
                                                         : TC_CREDENTIAL_UNAVAILABLE);
      } else if (status == TC_CREDENTIAL_UNAVAILABLE || status == TC_CREDENTIAL_REVOKED)
        tc_piv_check_status(run, &revocation, status);
      else
        tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
    }
  }
  card_cvc_check(run, &fields, signer, &cvc);
  tc_piv_check_add(run, &check);
  tc_piv_check_add(run, &revocation);
  tc_piv_check_add(run, &cvc);
}

#if TC_ENABLE_PIV_KEY_PROOF
static const struct {
  unsigned bit;
  size_t slot;
  uint8_t key;
  uint16_t container;
} proofs[] = {
    /* 9C first: PIN Always needs it right after the PIN (Part 1 Table 5). */
    {TC_PIV_CARD_PROVE_DIGITAL_SIGNATURE, TC_PIV_CARD_SLOT_DIGITAL_SIGNATURE,
     TC_PIV_KEY_DIGITAL_SIGNATURE, 0x0100},
    {TC_PIV_CARD_PROVE_PIV_AUTHENTICATION, TC_PIV_CARD_SLOT_PIV_AUTHENTICATION,
     TC_PIV_KEY_PIV_AUTHENTICATION, 0x0101},
    {TC_PIV_CARD_PROVE_CARD_AUTHENTICATION, TC_PIV_CARD_SLOT_CARD_AUTHENTICATION,
     TC_PIV_KEY_CARD_AUTHENTICATION, 0x0500},
};

enum {
  PROVE_KEYS = TC_PIV_CARD_PROVE_DIGITAL_SIGNATURE | TC_PIV_CARD_PROVE_PIV_AUTHENTICATION |
               TC_PIV_CARD_PROVE_CARD_AUTHENTICATION
};

static int proof_arguments_valid(const TC_PIV_link* link, const TC_PIV_card_proof_request* request,
                                 const TC_PIV_key_proof_workspace* workspace,
                                 const TC_work_budget* work, const TC_PIV_card_report* report)
{
  if (!link || !request || !workspace || !work || !report || !request->keys ||
      (request->keys & ~(unsigned)PROVE_KEYS) || report->count > TC_PIV_CARD_CHECKS_MAX ||
      request->policy.profile != report->profile ||
      (report->application == TC_PIV_APPLICATION_TWIC &&
       (request->keys & ~(unsigned)TC_PIV_CARD_PROVE_CARD_AUTHENTICATION)))
    return 0;
  const struct {
    const void* data;
    size_t size;
  } ranges[] = {{link, sizeof *link},
                {request, sizeof *request},
                {workspace, sizeof *workspace},
                {work, sizeof *work},
                {report, sizeof *report}};
  const size_t count = sizeof ranges / sizeof *ranges;
  for (size_t i = 0; i < count; ++i)
    for (size_t j = i + 1; j < count; ++j)
      if (!tc_internal_ranges_disjoint(ranges[i].data, ranges[i].size, ranges[j].data,
                                       ranges[j].size))
        return 0;
  return 1;
}

/* Record one proof result. Returns 0 for the results that abort. */
static int proof_result(const TC_PIV_link* link, TC_PIV_result result, TC_PIV_check* check)
{
  switch (result) {
  case TC_PIV_OK:
    return 1;
  case TC_PIV_INVALID:
    check->outcome = TC_PIV_CHECK_FAILED;
    check->status = TC_CREDENTIAL_INVALID;
    return 1;
  case TC_PIV_REFUSED:
    tc_piv_check_not_checkable(check, TC_PIV_REASON_RESTRICTED);
    return 1;
  case TC_PIV_CARD_STATUS:
    tc_piv_check_not_checkable(check, TC_PIV_REASON_CARD_STATUS);
    check->card_status = TC_PIV_link_status(link);
    return 1;
  case TC_PIV_UNSUPPORTED:
    check->status = TC_CREDENTIAL_UNSUPPORTED;
    tc_piv_check_not_checkable(check, TC_PIV_REASON_UNSUPPORTED);
    return 1;
  case TC_PIV_LIMIT:
    check->status = TC_CREDENTIAL_LIMIT;
    tc_piv_check_not_checkable(check, TC_PIV_REASON_LIMIT);
    return 1;
  default:
    return 0;
  }
}

TC_PIV_result TC_PIV_card_prove_keys(TC_PIV_link* link, const TC_PIV_card_proof_request* request,
                                     TC_PIV_key_proof_workspace* workspace, TC_work_budget* work,
                                     TC_PIV_card_report* report)
{
  if (!proof_arguments_valid(link, request, workspace, work, report))
    return TC_PIV_ARGUMENT;
  tc_piv_check_run run;
  memset(&run, 0, sizeof run);
  run.report = report;
  run.result = TC_PIV_OK;
  for (size_t i = 0; i < sizeof proofs / sizeof *proofs && run.result == TC_PIV_OK; ++i) {
    if (!(request->keys & proofs[i].bit))
      continue;
    TC_PIV_check check =
        tc_piv_check_make(TC_PIV_CHECK_KEY_PROOF, proofs[i].container, proofs[i].key);
    if (!report->certificate_valid[proofs[i].slot])
      tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
    else {
      const TC_PIV_key_proof_request proof = {&report->certificates[proofs[i].slot],
                                              request->policy, proofs[i].key};
      if (!proof_result(
              link,
              TC_PIV_key_prove(link, &proof, request->random, request->provider, workspace, work),
              &check)) {
        run.result = TC_PIV_ERROR;
        break;
      }
    }
    tc_piv_check_add(&run, &check);
  }
  TC_secure_zero(workspace, sizeof *workspace);
  if (run.result != TC_PIV_OK)
    TC_secure_zero(report, sizeof *report);
  return run.result;
}
#endif
#endif
