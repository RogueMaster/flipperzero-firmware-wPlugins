/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Card check report entries: construction, the status to outcome mapping,
 * lookup and the acceptance test. */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "piv_card_check_internal.h"
#include <string.h>

TC_PIV_check tc_piv_check_make(uint8_t kind, uint16_t container, uint8_t key_reference)
{
  TC_PIV_check check;
  memset(&check, 0, sizeof check);
  check.status = TC_CREDENTIAL_VALID;
  check.container = container;
  check.kind = kind;
  check.outcome = TC_PIV_CHECK_PASSED;
  check.reason = TC_PIV_REASON_NONE;
  check.key_reference = key_reference;
  return check;
}

const TC_PIV_check* tc_piv_check_add(tc_piv_check_run* run, const TC_PIV_check* check)
{
  if (run->result != TC_PIV_OK)
    return NULL;
  TC_PIV_card_report* report = run->report;
  if (report->count >= TC_PIV_CARD_CHECKS_MAX) {
    run->result = TC_PIV_LIMIT;
    return NULL;
  }
  report->checks[report->count] = *check;
  return &report->checks[report->count++];
}

void tc_piv_check_not_checkable(TC_PIV_check* check, uint8_t reason)
{
  check->outcome = TC_PIV_CHECK_NOT_CHECKABLE;
  check->reason = reason;
  if (check->status == TC_CREDENTIAL_VALID)
    check->status = TC_CREDENTIAL_UNAVAILABLE;
}

int tc_piv_check_status(tc_piv_check_run* run, TC_PIV_check* check, TC_credential_status status)
{
  check->status = status;
  switch (status) {
  case TC_CREDENTIAL_VALID:
    check->outcome = TC_PIV_CHECK_PASSED;
    check->reason = TC_PIV_REASON_NONE;
    return 1;
  case TC_CREDENTIAL_INVALID:
  case TC_CREDENTIAL_REVOKED:
    check->outcome = TC_PIV_CHECK_FAILED;
    check->reason = TC_PIV_REASON_NONE;
    return 1;
  case TC_CREDENTIAL_UNAVAILABLE:
    tc_piv_check_not_checkable(check, TC_PIV_REASON_NO_EVIDENCE);
    return 1;
  case TC_CREDENTIAL_UNSUPPORTED:
    tc_piv_check_not_checkable(check, TC_PIV_REASON_UNSUPPORTED);
    return 1;
  case TC_CREDENTIAL_LIMIT:
    tc_piv_check_not_checkable(check, TC_PIV_REASON_LIMIT);
    return 1;
  default:
    run->result = TC_PIV_ERROR;
    return 0;
  }
}

int tc_piv_check_tlv(tc_piv_check_run* run, TC_PIV_check* check, TC_TLV_result result)
{
  switch (result) {
  case TC_TLV_OK:
    return tc_piv_check_status(run, check, TC_CREDENTIAL_VALID);
  case TC_TLV_LIMIT:
    return tc_piv_check_status(run, check, TC_CREDENTIAL_LIMIT);
  case TC_TLV_UNSUPPORTED:
    return tc_piv_check_status(run, check, TC_CREDENTIAL_UNSUPPORTED);
  case TC_TLV_ARGUMENT:
  case TC_TLV_IO:
    return tc_piv_check_status(run, check, TC_CREDENTIAL_ERROR);
  default:
    return tc_piv_check_status(run, check, TC_CREDENTIAL_INVALID);
  }
}

void tc_piv_check_unread(const tc_piv_check_run* run, TC_PIV_check* check,
                         const TC_PIV_object* object, int missing_fails)
{
  uint8_t reason = TC_PIV_REASON_NOT_REQUESTED;
  int missing = 0;
  check->card_status = object->status;
  switch (object->state) {
  case TC_PIV_OBJECT_ABSENT:
    reason = TC_PIV_REASON_ABSENT;
    missing = 1;
    break;
  case TC_PIV_OBJECT_EMPTY:
    reason = TC_PIV_REASON_EMPTY;
    missing = 1;
    break;
  case TC_PIV_OBJECT_DENIED:
    reason = TC_PIV_REASON_DENIED;
    /* TWIC Part 2 v5 4.2: the PIV application of a TWIC card makes its
     * objects Never on contactless, so a denial there proves nothing. */
    missing = !(run->twic_piv && run->request->inventory->link.interface == TC_PIV_CONTACTLESS);
    break;
  case TC_PIV_OBJECT_RESTRICTED:
    reason = TC_PIV_REASON_RESTRICTED;
    break;
  case TC_PIV_OBJECT_OVERSIZED:
    reason = TC_PIV_REASON_OVERSIZED;
    break;
  default:
    break;
  }
  if (missing && missing_fails) {
    check->outcome = TC_PIV_CHECK_FAILED;
    check->reason = reason;
    check->status = TC_CREDENTIAL_INVALID;
    return;
  }
  tc_piv_check_not_checkable(check, reason);
}

int tc_piv_check_passed(const TC_PIV_card_report* report, uint8_t kind, uint16_t container)
{
  const TC_PIV_check_requirement requirement = {kind, 0, container};
  const TC_PIV_check* check = TC_PIV_card_report_find(report, &requirement);
  return check && check->outcome == TC_PIV_CHECK_PASSED;
}

const TC_PIV_object* tc_piv_check_object(const TC_PIV_inventory* inventory, uint8_t kind,
                                         uint8_t key_reference)
{
  for (size_t i = 0; i < inventory->count; ++i) {
    const TC_PIV_object_info* info = inventory->objects[i].info;
    if (info->kind == kind && (!key_reference || info->key_reference == key_reference))
      return &inventory->objects[i];
  }
  return NULL;
}

static int requirement_matches(const TC_PIV_check_requirement* requirement,
                               const TC_PIV_check* check)
{
  return check->kind == requirement->kind &&
         (!requirement->key_reference || check->key_reference == requirement->key_reference) &&
         (!requirement->container || check->container == requirement->container);
}

const TC_PIV_check* TC_PIV_card_report_find(const TC_PIV_card_report* report,
                                            const TC_PIV_check_requirement* requirement)
{
  if (!report || !requirement || report->count > TC_PIV_CARD_CHECKS_MAX)
    return NULL;
  for (size_t i = 0; i < report->count; ++i)
    if (requirement_matches(requirement, &report->checks[i]))
      return &report->checks[i];
  return NULL;
}

int TC_PIV_card_report_accepts(const TC_PIV_card_report* report,
                               const TC_PIV_check_requirement* required, size_t count)
{
  if (!report || !required || !count || report->count > TC_PIV_CARD_CHECKS_MAX)
    return 0;
  for (size_t r = 0; r < count; ++r) {
    size_t matched = 0;
    for (size_t i = 0; i < report->count; ++i) {
      if (!requirement_matches(&required[r], &report->checks[i]))
        continue;
      if (report->checks[i].outcome != TC_PIV_CHECK_PASSED)
        return 0;
      ++matched;
    }
    if (!matched)
      return 0;
  }
  return 1;
}
#endif
