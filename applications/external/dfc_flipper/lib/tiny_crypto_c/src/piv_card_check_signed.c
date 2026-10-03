/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Signed card objects: the CHUID, the Security Object and its per-container
 * digests, the biometric objects, the printed expiration and the Discovery
 * Object consistency. */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "piv_card_check_internal.h"
#include <tiny_crypto/piv_card_objects.h>
#include <tiny_crypto/piv_discovery.h>
#include <tiny_crypto/piv_printed.h>
#include <string.h>

/* Record a signer result and the REVOCATION entry beside it: VALID passes
 * the check and the revocation entry reports the evidence, UNAVAILABLE and
 * REVOKED apply to both, and other results leave revocation dependent.
 * Returns 0 when the run aborted. */
static int signer_result(tc_piv_check_run* run, TC_credential_status status, uint8_t checked,
                         TC_PIV_check* check, TC_PIV_check* revocation)
{
  if (!tc_piv_check_status(run, check, status))
    return 0;
  if (status == TC_CREDENTIAL_VALID)
    return tc_piv_check_status(run, revocation,
                               checked ? TC_CREDENTIAL_VALID : TC_CREDENTIAL_UNAVAILABLE);
  if (status == TC_CREDENTIAL_UNAVAILABLE || status == TC_CREDENTIAL_REVOKED)
    return tc_piv_check_status(run, revocation, status);
  tc_piv_check_not_checkable(revocation, TC_PIV_REASON_DEPENDENCY);
  return 1;
}

void tc_piv_check_chuid(tc_piv_check_run* run)
{
  if (run->result != TC_PIV_OK)
    return;
  TC_PIV_card_report* report = run->report;
  const TC_PIV_object* object = tc_piv_check_object(run->request->inventory, TC_PIV_KIND_CHUID, 0);
  if (!object)
    return;
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_CHUID, object->info->container, 0);
  TC_PIV_check revocation = tc_piv_check_make(TC_PIV_CHECK_REVOCATION, object->info->container, 0);
  if (object->state != TC_PIV_OBJECT_PRESENT) {
    tc_piv_check_unread(run, &check, object, 0);
    tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
  } else if (!report->has_card) {
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
    tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
  } else {
    /* SP 800-73-5 Part 1 section 3.1.2, TWIC Part 2 v5 section 4.6.3. The
     * PIV application of a TWIC card keeps the optional Authentication Key
     * Map of SP 800-73-2 (3D), which a NEXGEN card sends empty. */
    TC_PIV_CHUID_profile chuid_profile;
    if (TC_PIV_card_chuid_profile(run->report->application, report->profile, &chuid_profile) !=
        TC_TLV_OK) {
      run->result = TC_PIV_ERROR;
      return;
    }
    const TC_PIV_CHUID_validation_request request = {
        object->encoded, TC_PIV_CHUID_CONTAINER,  report->profile, chuid_profile, 0,
        &report->card,   &report->card_expiration};
    const TC_credential_status status =
        TC_PIV_CHUID_validate(&request, run->request->content, run->work, &report->chuid);
    if (!signer_result(run, status, report->chuid.revocation_checked, &check, &revocation))
      return;
    report->has_chuid = status == TC_CREDENTIAL_VALID;
  }
  tc_piv_check_add(run, &check);
  tc_piv_check_add(run, &revocation);
}

/* The container ID of mapping record index (group, then a big-endian ID). */
static uint16_t mapped_container(const TC_PIV_security_object* object, size_t index)
{
  const uint8_t* record = object->mapping.data + 3 * index;
  return (uint16_t)(record[1] << 8 | record[2]);
}

/* SECURITY_DIGEST of one mapped container. The signed map proves the object
 * exists, so a missing, denied or empty object fails. */
/* The TWIC application Security Object hashes the plaintext printed
 * information (TWIC Part 2 v5 4.6.5 note 1). The card stores it TPK
 * encrypted in DFC109, so its digest needs the TWIC Privacy Key. */
static int printed_encrypted(const tc_piv_check_run* run, uint16_t container)
{
  return container == 0x3001 && run->report->application == TC_PIV_APPLICATION_TWIC;
}

static void digest_check(tc_piv_check_run* run, uint16_t container, int authenticated)
{
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_SECURITY_DIGEST, container, 0);
  const TC_PIV_object* object = TC_PIV_inventory_find(run->request->inventory, container);
  if (!authenticated)
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
  else if (!object || printed_encrypted(run, container))
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_UNSUPPORTED);
  else if (object->state != TC_PIV_OBJECT_PRESENT)
    tc_piv_check_unread(run, &check, object, 1);
  else {
    /* The digest covers the value of the 53 container, or of 7E for the
     * Discovery Object (SP 800-73-5 Part 1 sections 3.1.7 and 3.3.2). */
    const TC_PIV_security_data data = {container, &object->value, 1};
    if (!tc_piv_check_status(
            run, &check, TC_PIV_security_digest_check(&run->report->security, &data, run->work)))
      return;
  }
  tc_piv_check_add(run, &check);
}

/* One SECURITY_DIGEST per record of the mapping, dependent unless the map
 * was authenticated. A mapping that does not read gets one dependent entry
 * without a container. */
static void digest_checks(tc_piv_check_run* run, const TC_PIV_object* object, int authenticated)
{
  TC_PIV_security_object parsed;
  const TC_PIV_security_object* map = &run->report->security.object;
  if (!authenticated) {
    if (object->state != TC_PIV_OBJECT_PRESENT ||
        TC_PIV_security_read(object->encoded, TC_PIV_SECURITY_CONTAINER, &parsed) != TC_TLV_OK) {
      digest_check(run, 0, 0);
      return;
    }
    map = &parsed;
  }
  for (size_t i = 0; i < map->mapping.length / 3 && run->result == TC_PIV_OK; ++i)
    digest_check(run, mapped_container(map, i), authenticated);
}

void tc_piv_check_security(tc_piv_check_run* run)
{
  if (run->result != TC_PIV_OK)
    return;
  TC_PIV_card_report* report = run->report;
  const TC_PIV_object* object =
      tc_piv_check_object(run->request->inventory, TC_PIV_KIND_SECURITY, 0);
  if (!object)
    return;
  TC_PIV_check check =
      tc_piv_check_make(TC_PIV_CHECK_SECURITY_SIGNATURE, object->info->container, 0);
  TC_PIV_check revocation = tc_piv_check_make(TC_PIV_CHECK_REVOCATION, object->info->container, 0);
  if (object->state != TC_PIV_OBJECT_PRESENT) {
    tc_piv_check_unread(run, &check, object, 0);
    tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
  } else if (!report->has_chuid) {
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
    tc_piv_check_not_checkable(&revocation, TC_PIV_REASON_DEPENDENCY);
  } else {
    /* SP 800-73-5 Part 1 section 3.1.7. */
    const TC_PIV_security_signature_request request = {object->encoded, TC_PIV_SECURITY_CONTAINER,
                                                       report->profile, &report->chuid,
                                                       &report->card_expiration};
    const TC_PIV_security_validation_workspace lds = {run->workspace->lds_content.data,
                                                      run->workspace->lds_content.capacity};
    const TC_credential_status status = TC_PIV_security_authenticate(
        &request, run->request->content, &lds, run->work, &report->security);
    if (!signer_result(run, status, report->security.revocation_checked, &check, &revocation))
      return;
    report->has_security = status == TC_CREDENTIAL_VALID;
  }
  tc_piv_check_add(run, &check);
  tc_piv_check_add(run, &revocation);
  digest_checks(run, object, report->has_security);
}

TC_TLV_result tc_piv_check_biometric_value(TC_bytes value, const TC_TLV_limits* limits,
                                           TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result status = TC_TLV_read(value, TC_TLV_ISO7816, limits, &element);
  if (status != TC_TLV_OK)
    return status;
  if (element.header.tag_length != 1 || element.header.tag[0] != 0xbc || !element.value.length)
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

static void biometric_check(tc_piv_check_run* run, uint8_t kind, TC_PIV_CBEFF_format format)
{
  if (run->result != TC_PIV_OK)
    return;
  const TC_PIV_card_report* report = run->report;
  const TC_PIV_object* object = tc_piv_check_object(run->request->inventory, kind, 0);
  if (!object)
    return;
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_BIOMETRIC, object->info->container, 0);
  /* Iris records are unsupported. The TWIC application encrypts its
   * biometrics with the TWIC Privacy Key (TWIC Part 2 v5 4.7). */
  if (format == TC_PIV_CBEFF_IRIS_IMAGE || report->application == TC_PIV_APPLICATION_TWIC)
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_UNSUPPORTED);
  else if (object->state != TC_PIV_OBJECT_PRESENT)
    tc_piv_check_unread(run, &check, object, 0);
  else if (!report->has_chuid)
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
  else {
    TC_bytes encoded = {NULL, 0};
    const TC_TLV_result read = tc_piv_check_biometric_value(
        object->value, &run->request->content->options->parsing, &encoded);
    if (read != TC_TLV_OK) {
      if (!tc_piv_check_tlv(run, &check, read))
        return;
    } else {
      /* SP 800-76-2 sections 9.2 and 9.3. */
      const TC_PIV_biometric_validation_request request = {encoded,
                                                           report->profile,
                                                           &report->chuid,
                                                           &report->card_expiration,
                                                           TC_PIV_CMS_BIOMETRIC,
                                                           format,
                                                           0};
      TC_PIV_biometric_report result;
      if (!tc_piv_check_status(
              run, &check,
              TC_PIV_biometric_validate(&request, run->request->content, run->work, &result)))
        return;
    }
  }
  tc_piv_check_add(run, &check);
}

void tc_piv_check_biometrics(tc_piv_check_run* run)
{
  biometric_check(run, TC_PIV_KIND_FINGERPRINTS, TC_PIV_CBEFF_FINGERPRINT_TEMPLATE);
  biometric_check(run, TC_PIV_KIND_FACE, TC_PIV_CBEFF_FACE_IMAGE);
  biometric_check(run, TC_PIV_KIND_IRIS, TC_PIV_CBEFF_IRIS_IMAGE);
}

/* PRINTED_EXPIRATION: the authenticated printed expiration equals the CHUID
 * expiration and is current (SP 800-73-5 Part 1 section 3.3.1). */
void tc_piv_check_printed(tc_piv_check_run* run)
{
  if (run->result != TC_PIV_OK)
    return;
  const TC_PIV_card_report* report = run->report;
  const TC_PIV_object* object =
      tc_piv_check_object(run->request->inventory, TC_PIV_KIND_PRINTED, 0);
  if (!object)
    return;
  const uint16_t container = object->info->container;
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_PRINTED_EXPIRATION, container, 0);
  if (report->application == TC_PIV_APPLICATION_TWIC)
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_UNSUPPORTED);
  else if (object->state != TC_PIV_OBJECT_PRESENT)
    tc_piv_check_unread(run, &check, object, 0);
  else if (!report->has_chuid ||
           !tc_piv_check_passed(report, TC_PIV_CHECK_SECURITY_DIGEST, container))
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
  else {
    TC_PIV_printed printed;
    int current = 0;
    TC_TLV_result status = TC_PIV_printed_read(object->encoded, TC_PIV_PRINTED_CONTAINER,
                                               TC_PIV_PRINTED_PROFILE_PIV, &printed);
    if (status == TC_TLV_OK)
      status = TC_PIV_printed_expiration_check(&printed, report->chuid.object.expiration,
                                               &report->at, &current);
    if (status == TC_TLV_OK && !current)
      status = TC_TLV_INVALID;
    if (!tc_piv_check_tlv(run, &check, status))
      return;
  }
  tc_piv_check_add(run, &check);
}

/* 1 when the issuer's integrity rule holds for object: the Security Object
 * is authenticated, and a container its map names passed its digest. */
static int integrity_proven(const tc_piv_check_run* run, const TC_PIV_object* object)
{
  if (!run->report->has_security)
    return 0;
  const TC_PIV_check_requirement digest = {TC_PIV_CHECK_SECURITY_DIGEST, 0,
                                           object->info->container};
  const TC_PIV_check* digested = TC_PIV_card_report_find(run->report, &digest);
  return digested && digested->outcome == TC_PIV_CHECK_PASSED;
}

/* DISCOVERY_CONSISTENCY: a nonempty BIT group needs the OCC bit of the
 * Discovery Object PIN usage policy (SP 800-73-5 Part 1 sections 3.3.2 and
 * 3.3.6). Both objects get their integrity from the Security Object, so the
 * check needs an authenticated map and a passed digest of each object the
 * map names. */
void tc_piv_check_discovery(tc_piv_check_run* run)
{
  if (run->result != TC_PIV_OK)
    return;
  const TC_PIV_inventory* inventory = run->request->inventory;
  const TC_PIV_object* discovery = tc_piv_check_object(inventory, TC_PIV_KIND_DISCOVERY, 0);
  const TC_PIV_object* bit_group = tc_piv_check_object(inventory, TC_PIV_KIND_BIT_GROUP, 0);
  if (!discovery || !bit_group)
    return;
  const uint16_t container = discovery->info->container;
  TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_DISCOVERY_CONSISTENCY, container, 0);
  if (discovery->state != TC_PIV_OBJECT_PRESENT)
    tc_piv_check_unread(run, &check, discovery, 0);
  else if (bit_group->state != TC_PIV_OBJECT_PRESENT && bit_group->state != TC_PIV_OBJECT_ABSENT &&
           bit_group->state != TC_PIV_OBJECT_EMPTY)
    tc_piv_check_unread(run, &check, bit_group, 0);
  else if (!integrity_proven(run, discovery) ||
           (bit_group->state == TC_PIV_OBJECT_PRESENT && !integrity_proven(run, bit_group)))
    tc_piv_check_not_checkable(&check, TC_PIV_REASON_DEPENDENCY);
  else {
    TC_PIV_discovery policy;
    const TC_PIV_discovery_profile profile =
        tc_piv_check_piv(run) ? TC_PIV_DISCOVERY_PIV : TC_PIV_DISCOVERY_TWIC;
    TC_TLV_result status = TC_PIV_discovery_read(discovery->encoded, profile, &policy);
    if (status == TC_TLV_OK && (policy.policy & TC_PIV_POLICY_OCC) &&
        bit_group->state != TC_PIV_OBJECT_PRESENT)
      status = TC_TLV_INVALID;
    if (status == TC_TLV_OK && bit_group->state == TC_PIV_OBJECT_PRESENT) {
      TC_PIV_bit_group group;
      status = TC_PIV_bit_group_read(bit_group->encoded, &group);
      if (status == TC_TLV_OK && group.fingers && !(policy.policy & TC_PIV_POLICY_OCC))
        status = TC_TLV_INVALID;
    }
    if (!tc_piv_check_tlv(run, &check, status))
      return;
  }
  tc_piv_check_add(run, &check);
}
#endif
