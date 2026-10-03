/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * PIV and TWIC Security Object validation (SP 800-73-5 Part 1 section
 * 3.1.7): signature authentication into a container map, per-container
 * digest checks and the complete-inventory validator built on both. */
#include <tiny_crypto/credential.h>

#if TC_ENABLE_CREDENTIAL
#include "internal.h"
#include "credential_session_internal.h"
#include "credential_policy_internal.h"
#include "credential_status_internal.h"
#include "pki_budget_internal.h"

/* The LDS hash sequence nests two levels: SEQUENCE OF DataGroupHash. */
enum { SECURITY_DIGEST_FRAMES = 4 };

/* Extra storage of the complete-inventory validator: its inventory inputs
 * and its public result, checked with the authentication preflight. */
typedef struct {
  const TC_PIV_security_data* objects;
  size_t count;
  void* out;
  size_t out_size;
} security_inventory;

static int security_request_valid(const TC_PIV_security_signature_request* request,
                                  const TC_PIV_security_validation_workspace* workspace)
{
  return request && request->encoded.data && request->encoded.length && request->card_expiration &&
         workspace && workspace->content && workspace->content_capacity &&
         (request->encoding == TC_PIV_SECURITY_CONTENTS ||
          request->encoding == TC_PIV_SECURITY_CONTAINER);
}

/* Authenticate the Security Object and decode its map and LDS digests.
 * inventory adds the complete-inventory inputs and result to the storage
 * preflight, or is NULL. The caller checked request, workspace and work,
 * opened session and checked the CHUID binding. */
static TC_credential_status security_authenticate(
    tc_credential_session* session, const TC_PIV_security_signature_request* request,
    const TC_validation_context* context, const TC_PIV_security_validation_workspace* workspace,
    const security_inventory* inventory, size_t* work, TC_PIV_security_map* out)
{
  const TC_bytes signer_bytes = request->chuid->signer;
  const TC_bytes inputs[] = {
      request->encoded,
      signer_bytes,
      {(const uint8_t*)request, sizeof *request},
      {(const uint8_t*)request->chuid, sizeof *request->chuid},
      {(const uint8_t*)workspace, sizeof *workspace},
      {(const uint8_t*)request->card_expiration, sizeof *request->card_expiration}};
  /* The content decoder writes only after every signature and inventory
   * input has been checked for overlap with its buffer. */
  const tc_credential_storage storage = {inputs,
                                         sizeof inputs / sizeof *inputs,
                                         inventory ? inventory->out : out,
                                         inventory ? inventory->out_size : sizeof *out,
                                         workspace->content,
                                         workspace->content_capacity,
                                         inventory ? inventory->objects : NULL,
                                         inventory ? inventory->count : 0};
  TC_TLV_result parsed = tc_credential_session_bind(session, context, &storage, work);
  if (parsed == TC_TLV_OK)
    parsed = tc_credential_session_input(session, request->encoded, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_TLV_limits* limits = &session->policy.path.parsing;
  const TC_X509_path_workspace* scratch = tc_credential_scratch(context);

  TC_PIV_security_object container;
  TC_PIV_CMS_object object = {0};
  TC_X509_certificate signer = {0};
  session->policy.verification.attribute_oids = tc_credential_attribute_oids(session->oids);
  parsed = TC_PIV_security_read(request->encoded, request->encoding, &container);
  if (parsed == TC_TLV_OK)
    parsed = TC_PIV_CMS_read(container.cms, TC_PIV_CMS_SECURITY, &session->policy.verification,
                             limits, tc_credential_frames(context), work, &object);
  if (parsed == TC_TLV_OK)
    parsed = tc_credential_signer_read(signer_bytes, limits, scratch, work, &signer);
  if (parsed == TC_TLV_OK)
    parsed =
        tc_credential_signer_policy(&signer, session->piv, !session->piv, request->card_expiration,
                                    &session->policy.path, scratch, work);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  const TC_CMS_validation_request cms = {container.cms, 0, object.envelope.content_type,
                                         NULL,          0, signer_bytes};
  TC_credential_status status = tc_credential_session_verify(session, context, &cms, &object, work);
  if (status != TC_CREDENTIAL_VALID)
    return status;
  TC_LDS_security_object lds;
  parsed = TC_LDS_read_content(object.envelope.content, limits, tc_credential_frames(context), work,
                               (TC_buffer){workspace->content, workspace->content_capacity}, &lds);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  /* The container map and the signed LDS must name the same data groups. */
  if (container.groups != lds.groups)
    return TC_CREDENTIAL_INVALID;
  const TC_PIV_security_map map = {container,
                                   lds,
                                   *limits,
                                   signer_bytes,
                                   request->profile,
                                   context->options->at,
                                   session->revocation_checked};
  *out = map;
  return TC_CREDENTIAL_VALID;
}

TC_credential_status TC_PIV_security_authenticate(
    const TC_PIV_security_signature_request* request, const TC_validation_context* context,
    const TC_PIV_security_validation_workspace* workspace, size_t* work, TC_PIV_security_map* out)
{
  tc_credential_session session;
  if (!security_request_valid(request, workspace) || !work || !out ||
      !tc_credential_session_open(&session, context, request->profile) ||
      !tc_credential_chuid_bound(request->chuid, request->profile, context))
    return TC_CREDENTIAL_ERROR;
  return security_authenticate(&session, request, context, workspace, NULL, work, out);
}

/* Hash one object and compare it with the signed digest of its container.
 * group receives the data group number on VALID and INVALID. The caller
 * checked storage. */
static TC_credential_status security_digest(const TC_PIV_security_map* map,
                                            const TC_PIV_security_data* object,
                                            TC_TLV_frames frames, size_t* work, unsigned* group)
{
  unsigned number = 0;
  int matched = 0;
  TC_TLV_result parsed = TC_PIV_security_group_find(&map->object, object->container, &number);
  if (parsed == TC_TLV_END)
    return TC_CREDENTIAL_UNAVAILABLE;
  if (parsed == TC_TLV_OK)
    parsed = TC_LDS_hash_check(&map->lds, number, object->parts, object->count, &map->limits,
                               frames, work, &matched);
  if (parsed != TC_TLV_OK)
    return tc_validation_status(parsed);
  *group = number;
  return matched ? TC_CREDENTIAL_VALID : TC_CREDENTIAL_INVALID;
}

TC_credential_status TC_PIV_security_digest_check(const TC_PIV_security_map* map,
                                                  const TC_PIV_security_data* object, size_t* work)
{
  if (!map || !object || !work || !object->parts || !object->count ||
      map->object.groups != map->lds.groups)
    return TC_CREDENTIAL_ERROR;
  /* work is the only write. Frames and hash scratch live on this stack. */
  TC_bytes counter;
  tc_pki_storage_plan plan;
  tc_pki_storage_plan_begin(&plan, &counter, 1, *work);
  TC_PKI_PLAN_WRITE(&plan, work, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, map, 1);
  TC_PKI_PLAN_INPUT(&plan, object, 1);
  TC_PKI_PLAN_INPUT(&plan, object->parts, object->count);
  tc_pki_storage_plan_input_span(&plan, map->object.mapping);
  tc_pki_storage_plan_input_span(&plan, map->lds.encoded);
  tc_pki_storage_plan_input_spans(&plan, object->parts, object->count);
  TC_TLV_result stored = tc_pki_storage_plan_finish(&plan, work);
  if (stored != TC_TLV_OK)
    return tc_validation_status(stored);
  TC_TLV_frame frames[SECURITY_DIGEST_FRAMES];
  unsigned group = 0;
  return security_digest(map, object, (TC_TLV_frames){frames, SECURITY_DIGEST_FRAMES}, work,
                         &group);
}

TC_credential_status TC_PIV_security_validate(const TC_PIV_security_validation_request* request,
                                              const TC_validation_context* context,
                                              const TC_PIV_security_validation_workspace* workspace,
                                              size_t* work, TC_PIV_security_report* out)
{
  if (request && request->count > TC_LDS_MAX_GROUPS)
    return TC_CREDENTIAL_LIMIT;
  if (!request || !request->objects || !request->count || !out)
    return TC_CREDENTIAL_ERROR;
  const TC_PIV_security_signature_request signature = {request->encoded, request->encoding,
                                                       request->profile, request->chuid,
                                                       request->card_expiration};
  tc_credential_session session;
  if (!security_request_valid(&signature, workspace) || !work ||
      !tc_credential_session_open(&session, context, request->profile) ||
      !tc_credential_chuid_bound(request->chuid, request->profile, context))
    return TC_CREDENTIAL_ERROR;
  if (request->count < 2)
    return TC_CREDENTIAL_INVALID;
  for (size_t i = 0; i < request->count; ++i) {
    if (!request->objects[i].parts || !request->objects[i].count)
      return TC_CREDENTIAL_ERROR;
    for (size_t j = 0; j < i; ++j)
      if (request->objects[i].container == request->objects[j].container)
        return TC_CREDENTIAL_INVALID;
  }
  const security_inventory inventory = {request->objects, request->count, out, sizeof *out};
  TC_PIV_security_map map;
  TC_credential_status status =
      security_authenticate(&session, &signature, context, workspace, &inventory, work, &map);
  if (status != TC_CREDENTIAL_VALID)
    return status;
  /* Every signed group must be supplied exactly once. A container outside
   * the signed map fails the inventory. */
  uint16_t checked = 0;
  for (size_t i = 0; i < request->count; ++i) {
    unsigned group = 0;
    status =
        security_digest(&map, &request->objects[i], tc_credential_frames(context), work, &group);
    if (status == TC_CREDENTIAL_UNAVAILABLE)
      return TC_CREDENTIAL_INVALID;
    if (status != TC_CREDENTIAL_VALID)
      return status;
    const uint16_t bit = (uint16_t)(1u << (group - 1));
    if (checked & bit)
      return TC_CREDENTIAL_INVALID;
    checked |= bit;
  }
  if (checked != map.lds.groups)
    return TC_CREDENTIAL_INVALID;
  const TC_PIV_security_report accepted = {request->objects, request->count,
                                           map.signer,       request->profile,
                                           map.at,           map.revocation_checked};
  *out = accepted;
  return TC_CREDENTIAL_VALID;
}

#endif
