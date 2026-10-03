/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The composed card check: argument validation, the phase order, the
 * mandatory object and copy checks. The certificate, signed object and
 * secure messaging phases live in their own files. */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "internal.h"
#include "piv_card_check_internal.h"
#include <string.h>

static int context_complete(const TC_validation_context* context)
{
  return context && context->options && context->workspace && context->workspace->path &&
         context->trust.certificates && context->trust.crls;
}

static int buffer_valid(TC_buffer buffer)
{
  return buffer.data || !buffer.capacity;
}

/* The credential profile fits the inventory application: the TWIC
 * application keeps its SELECT profile, and the PIV application takes the
 * PIV profile or a TWIC profile for a TWIC card. */
static int profile_fits(TC_PIV_card_profile profile, const TC_PIV_inventory* inventory)
{
  if (profile != TC_PIV_CARD && profile != TC_TWIC_LEGACY_CARD && profile != TC_TWIC_NEXGEN_CARD)
    return 0;
  if (inventory->link.application == TC_PIV_APPLICATION_TWIC)
    return profile == inventory->link.profile;
  return inventory->link.application == TC_PIV_APPLICATION_PIV;
}

TC_TLV_result TC_PIV_card_chuid_profile(TC_PIV_application_id application,
                                        TC_PIV_card_profile profile, TC_PIV_CHUID_profile* out)
{
  if (!out ||
      (profile != TC_PIV_CARD && profile != TC_TWIC_LEGACY_CARD && profile != TC_TWIC_NEXGEN_CARD))
    return TC_TLV_ARGUMENT;
  if (application == TC_PIV_APPLICATION_PIV)
    *out = profile == TC_PIV_CARD ? TC_CHUID_PROFILE_PIV : TC_CHUID_PROFILE_LEGACY_KEY_MAP;
  else if (application == TC_PIV_APPLICATION_TWIC && profile != TC_PIV_CARD)
    *out = TC_CHUID_PROFILE_TWIC_SIGNED;
  else
    return TC_TLV_ARGUMENT;
  return TC_TLV_OK;
}

static int objects_valid(const TC_PIV_object* objects, size_t count)
{
  if (count && !objects)
    return 0;
  for (size_t i = 0; i < count; ++i)
    if (!objects[i].info)
      return 0;
  return 1;
}

/* A card check consumes the complete catalog snapshot produced by
 * TC_PIV_inventory_read. Exact pointer identity also prevents callers from
 * substituting metadata with the same container number. */
static int inventory_complete(const TC_PIV_inventory* inventory)
{
  const size_t expected =
      TC_PIV_catalog_count(inventory->link.application, inventory->link.profile);
  if (!expected || inventory->count != expected || inventory->capacity < expected ||
      !inventory->objects || (!inventory->pool && inventory->pool_used))
    return 0;
  for (size_t i = 0; i < expected; ++i)
    if (inventory->objects[i].info !=
        TC_PIV_catalog_at(inventory->link.application, inventory->link.profile, i))
      return 0;
  return 1;
}

static int disjoint(const void* first, size_t first_size, const void* second, size_t second_size)
{
  return tc_internal_ranges_disjoint(first, first_size, second, second_size);
}

/* Both write ranges are disjoint from span. */
static int writes_disjoint(const void* data, size_t size, const size_t* work,
                           const TC_PIV_card_report* out)
{
  return disjoint(data, size, out, sizeof *out) && disjoint(data, size, work, sizeof *work);
}

/* The bytes of the plain copies and the OCSP responses stay outside the
 * write ranges. */
static int evidence_disjoint(const TC_PIV_card_check_request* request, const size_t* work,
                             const TC_PIV_card_report* out)
{
  for (size_t i = 0; i < request->plain_copy_count; ++i)
    if (!writes_disjoint(request->plain_copies[i].encoded.data,
                         request->plain_copies[i].encoded.length, work, out))
      return 0;
  for (size_t i = 0; request->ocsp && i < TC_PIV_CARD_CERTIFICATES; ++i)
    if (!writes_disjoint(request->ocsp->responses[i].data, request->ocsp->responses[i].length, work,
                         out))
      return 0;
  return 1;
}

/* Every write range (out, *work) is disjoint from every input and from the
 * workspace, and the workspace buffers are disjoint from each other and
 * from the inventory pool. */
static int storage_disjoint(const TC_PIV_card_check_request* request,
                            const TC_PIV_card_check_workspace* workspace, const size_t* work,
                            const TC_PIV_card_report* out)
{
  const TC_PIV_inventory* inventory = request->inventory;
  const struct {
    const void* data;
    size_t size;
  } inputs[] = {{request, sizeof *request},
                {inventory, sizeof *inventory},
                {inventory->objects, inventory->count * sizeof *inventory->objects},
                {inventory->pool, inventory->pool_used},
                {request->plain_copies, request->plain_copy_count * sizeof *request->plain_copies},
                {request->card, sizeof *request->card},
                {request->content, sizeof *request->content},
                {request->ocsp, request->ocsp ? sizeof *request->ocsp : 0},
                {request->link, request->link ? sizeof *request->link : 0},
                {request->sm_card_cvc.data, request->sm_card_cvc.length},
                {workspace, sizeof *workspace},
                {workspace->certificates.data, workspace->certificates.capacity},
                {workspace->lds_content.data, workspace->lds_content.capacity}};
  if (!disjoint(work, sizeof *work, out, sizeof *out))
    return 0;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
    if (!writes_disjoint(inputs[i].data, inputs[i].size, work, out))
      return 0;
  if (!evidence_disjoint(request, work, out))
    return 0;
  const TC_buffer buffers[] = {workspace->certificates, workspace->lds_content};
  for (size_t i = 0; i < 2; ++i)
    if (!disjoint(buffers[i].data, buffers[i].capacity, inventory->pool, inventory->pool_used) ||
        !disjoint(buffers[i].data, buffers[i].capacity, request, sizeof *request))
      return 0;
  return disjoint(buffers[0].data, buffers[0].capacity, buffers[1].data, buffers[1].capacity);
}

static int arguments_valid(const TC_PIV_card_check_request* request,
                           const TC_PIV_card_check_workspace* workspace, const size_t* work,
                           const TC_PIV_card_report* out)
{
  if (!request || !workspace || !work || !out || !request->inventory)
    return 0;
  const TC_PIV_inventory* inventory = request->inventory;
  int order = 1;
  return inventory_complete(inventory) &&
         objects_valid(request->plain_copies, request->plain_copy_count) &&
         request->plain_copy_count <= SIZE_MAX / sizeof *request->plain_copies &&
         (request->sm_card_cvc.data || !request->sm_card_cvc.length) &&
         context_complete(request->card) && context_complete(request->content) &&
         TC_X509_time_compare(&request->card->options->at, &request->content->options->at,
                              &order) == TC_TLV_OK &&
         !order && profile_fits(request->profile, inventory) &&
         tc_piv_check_card_purpose_valid(request->card->options, request->profile) &&
         buffer_valid(workspace->certificates) && buffer_valid(workspace->lds_content) &&
         storage_disjoint(request, workspace, work, out);
}

/* MANDATORY_OBJECT for each mandatory catalog entry. */
static void mandatory_objects(tc_piv_check_run* run)
{
  const TC_PIV_inventory* inventory = run->request->inventory;
  for (size_t i = 0; i < inventory->count && run->result == TC_PIV_OK; ++i) {
    const TC_PIV_object* object = &inventory->objects[i];
    if (object->info->requirement != TC_PIV_MANDATORY)
      continue;
    TC_PIV_check check =
        tc_piv_check_make(TC_PIV_CHECK_MANDATORY_OBJECT, object->info->container, 0);
    if (object->state != TC_PIV_OBJECT_PRESENT)
      tc_piv_check_unread(run, &check, object, 1);
    tc_piv_check_add(run, &check);
  }
}

static int read_in_full(const TC_PIV_object* object)
{
  return object->state == TC_PIV_OBJECT_PRESENT || object->state == TC_PIV_OBJECT_EMPTY;
}

/* COPY_MATCH: each plain copy equals the inventory entry of its container
 * byte for byte. */
static void copy_matches(tc_piv_check_run* run)
{
  const TC_PIV_card_check_request* request = run->request;
  for (size_t i = 0; i < request->plain_copy_count && run->result == TC_PIV_OK; ++i) {
    const TC_PIV_object* copy = &request->plain_copies[i];
    const TC_PIV_object* object = TC_PIV_inventory_find(request->inventory, copy->info->container);
    TC_PIV_check check = tc_piv_check_make(TC_PIV_CHECK_COPY_MATCH, copy->info->container, 0);
    if (!read_in_full(copy))
      tc_piv_check_unread(run, &check, copy, 0);
    else if (!object)
      tc_piv_check_not_checkable(&check, TC_PIV_REASON_NOT_REQUESTED);
    else if (!read_in_full(object))
      tc_piv_check_unread(run, &check, object, 0);
    else if (copy->encoded.length != object->encoded.length ||
             (copy->encoded.length &&
              memcmp(copy->encoded.data, object->encoded.data, copy->encoded.length)))
      tc_piv_check_status(run, &check, TC_CREDENTIAL_INVALID);
    tc_piv_check_add(run, &check);
  }
}

TC_PIV_result TC_PIV_card_check(const TC_PIV_card_check_request* request,
                                TC_PIV_card_check_workspace* workspace, size_t* work,
                                TC_PIV_card_report* out)
{
  if (!arguments_valid(request, workspace, work, out))
    return TC_PIV_ARGUMENT;
  memset(out, 0, sizeof *out);
  out->profile = request->profile;
  out->application = request->inventory->link.application;
  out->at = request->content->options->at;
  tc_piv_check_run run = {request, workspace, work, out, 0, TC_PIV_OK, 0};
  run.twic_piv = request->profile != TC_PIV_CARD &&
                 request->inventory->link.application == TC_PIV_APPLICATION_PIV;

  mandatory_objects(&run);
  tc_piv_check_card_certificate(&run);
  tc_piv_check_chuid(&run);
  tc_piv_check_key_certificates(&run);
  tc_piv_check_security(&run);
  tc_piv_check_biometrics(&run);
  tc_piv_check_printed(&run);
  tc_piv_check_discovery(&run);
  tc_piv_check_secure_messaging(&run);
  copy_matches(&run);
  if (run.result != TC_PIV_OK) {
    TC_secure_zero(out, sizeof *out);
    return run.result;
  }
  return TC_PIV_OK;
}
#endif
