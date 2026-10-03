/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CRL lookup targets of a read inventory: the issuer and serial of every
 * certificate a card check may query for revocation (RFC 5280 section
 * 5.3.3). */
#include <tiny_crypto/piv_card_check.h>
#if TC_ENABLE_PIV_CARD_CHECK
#include "internal.h"
#include "piv_card_check_internal.h"
#include <tiny_crypto/cms.h>
#include <tiny_crypto/piv_chuid.h>
#include <tiny_crypto/piv_cms.h>
#include <tiny_crypto/piv_security.h>
#include <string.h>

enum { CERTIFICATE_TAG = 0x30 };

typedef struct {
  const TC_TLV_limits* limits;
  TC_PIV_crl_target_workspace* workspace;
  size_t* work;
  TC_X509_crl_target* targets;
  size_t capacity, count;
  /* Bytes of workspace->certificates that hold decoded certificates. */
  size_t certificate_used;
  TC_PIV_result result;
} target_list;

static int span_equal(TC_bytes a, TC_bytes b)
{
  return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static int charge(target_list* list, size_t units)
{
  if (units > *list->work) {
    list->result = TC_PIV_LIMIT;
    return 0;
  }
  *list->work -= units;
  return 1;
}

/* Record a read result: LIMIT means the limits or budgets are too small and
 * stops the listing. Returns 1 when the read succeeded. */
static int read_ok(target_list* list, TC_TLV_result status)
{
  if (status == TC_TLV_LIMIT)
    list->result = TC_PIV_LIMIT;
  return status == TC_TLV_OK;
}

/* Add the issuer and serial of one encoded certificate. A certificate that
 * does not parse adds nothing. */
static void certificate_add(target_list* list, TC_bytes encoded)
{
  if (list->result != TC_PIV_OK || !charge(list, encoded.length))
    return;
  TC_X509_certificate certificate;
  if (!read_ok(list, TC_X509_read(encoded, list->limits, &list->workspace->parsing, &certificate)))
    return;
  const TC_X509_crl_target target = {certificate.serial, certificate.issuer};
  for (size_t i = 0; i < list->count; ++i)
    if (span_equal(list->targets[i].serial, target.serial) &&
        span_equal(list->targets[i].issuer, target.issuer))
      return;
  if (list->count == list->capacity) {
    list->result = TC_PIV_LIMIT;
    return;
  }
  list->targets[list->count++] = target;
}

/* Every X.509 certificate of a CMS SignedData certificate set (RFC 5652
 * section 10.2.3). Other choices are skipped. */
static void cms_add(target_list* list, TC_bytes cms)
{
  static const TC_CMS_verification_policy envelope = {.envelope = TC_CMS_ENVELOPE_BER};
  TC_CMS_signed_data data;
  if (list->result != TC_PIV_OK || !cms.length ||
      !read_ok(list, TC_CMS_signed_data_read(cms, &envelope, list->limits,
                                             list->workspace->parsing.frames, list->work, &data)) ||
      !data.certificates.length)
    return;
  TC_TLV_element set, certificate;
  TC_TLV_reader reader;
  if (!read_ok(list, TC_TLV_read(data.certificates, TC_TLV_BER, list->limits, &set)) ||
      !read_ok(list, TC_TLV_reader_init(&reader, set.value, TC_TLV_BER, list->limits)))
    return;
  while (list->result == TC_PIV_OK && TC_TLV_next(&reader, &certificate) == TC_TLV_OK)
    if (certificate.header.tag_length == 1 && certificate.header.tag[0] == CERTIFICATE_TAG)
      certificate_add(list, certificate.encoded);
}

/* A certificate container, decoded into the next free certificate bytes
 * when compressed. */
static void container_add(target_list* list, const TC_PIV_object* object,
                          TC_PIV_certificate_profile profile)
{
  TC_buffer* storage = &list->workspace->certificates;
  const TC_buffer der = {storage->data ? storage->data + list->certificate_used : NULL,
                         storage->capacity - list->certificate_used};
  TC_PIV_certificate container;
  if (!read_ok(list,
               TC_PIV_certificate_decode(object->encoded, profile, list->limits->max_input,
                                         &list->workspace->gzip, list->work, der, &container)))
    return;
  if (container.certificate.data == der.data)
    list->certificate_used += container.certificate.length;
  certificate_add(list, container.certificate);
}

/* The CMS signature of a signed object, or an empty span. */
static TC_bytes object_cms(const TC_PIV_object* object, TC_PIV_application_id application,
                           const TC_TLV_limits* limits)
{
  const TC_bytes none = {NULL, 0};
  switch (object->info->kind) {
  case TC_PIV_KIND_CHUID: {
    /* The PIV application of a TWIC card keeps the SP 800-73-2 key map. */
    TC_PIV_CHUID chuid;
    const TC_PIV_CHUID_profile profile = application == TC_PIV_APPLICATION_TWIC
                                             ? TC_CHUID_PROFILE_TWIC_SIGNED
                                             : TC_CHUID_PROFILE_LEGACY_KEY_MAP;
    return TC_PIV_CHUID_read(object->encoded, TC_PIV_CHUID_CONTAINER, profile, &chuid) == TC_TLV_OK
               ? chuid.signature
               : none;
  }
  case TC_PIV_KIND_SECURITY: {
    TC_PIV_security_object security;
    return TC_PIV_security_read(object->encoded, TC_PIV_SECURITY_CONTAINER, &security) == TC_TLV_OK
               ? security.cms
               : none;
  }
  case TC_PIV_KIND_FINGERPRINTS:
  case TC_PIV_KIND_FACE:
  case TC_PIV_KIND_IRIS: {
    /* TWIC Privacy Key encrypted records do not read as CBEFF. */
    TC_bytes record;
    TC_PIV_CBEFF cbeff;
    return tc_piv_check_biometric_value(object->value, limits, &record) == TC_TLV_OK &&
                   TC_PIV_CBEFF_read(record, &cbeff) == TC_TLV_OK
               ? cbeff.signature
               : none;
  }
  default:
    return none;
  }
}

static int arguments_valid(const TC_PIV_inventory* inventory, const TC_TLV_limits* limits,
                           const TC_PIV_crl_target_workspace* workspace, const size_t* work,
                           const TC_X509_crl_target* targets, size_t capacity, const size_t* count)
{
  if (!inventory || (inventory->count && !inventory->objects) || !limits || !workspace || !work ||
      !count || (!targets && capacity) ||
      (!workspace->certificates.data && workspace->certificates.capacity) ||
      capacity > SIZE_MAX / sizeof *targets)
    return 0;
  const TC_bytes outputs[] = {{(const uint8_t*)targets, capacity * sizeof *targets},
                              {(const uint8_t*)count, sizeof *count},
                              {(const uint8_t*)work, sizeof *work},
                              {(const uint8_t*)workspace, sizeof *workspace},
                              {workspace->certificates.data, workspace->certificates.capacity}};
  const TC_bytes inputs[] = {
      {(const uint8_t*)inventory, sizeof *inventory},
      {(const uint8_t*)inventory->objects, inventory->count * sizeof *inventory->objects},
      {inventory->pool, inventory->pool_used},
      {(const uint8_t*)limits, sizeof *limits}};
  const size_t output_count = sizeof outputs / sizeof *outputs;
  for (size_t i = 0; i < output_count; ++i) {
    for (size_t j = i + 1; j < output_count; ++j)
      if (!tc_internal_ranges_disjoint(outputs[i].data, outputs[i].length, outputs[j].data,
                                       outputs[j].length))
        return 0;
    for (size_t j = 0; j < sizeof inputs / sizeof *inputs; ++j)
      if (!tc_internal_ranges_disjoint(outputs[i].data, outputs[i].length, inputs[j].data,
                                       inputs[j].length))
        return 0;
  }
  return 1;
}

TC_PIV_result TC_PIV_card_crl_targets(const TC_PIV_inventory* inventory,
                                      const TC_TLV_limits* limits,
                                      TC_PIV_crl_target_workspace* workspace, size_t* work,
                                      TC_X509_crl_target* targets, size_t capacity, size_t* count)
{
  if (!arguments_valid(inventory, limits, workspace, work, targets, capacity, count))
    return TC_PIV_ARGUMENT;
  target_list list = {limits, workspace, work, targets, capacity, 0, 0, TC_PIV_OK};
  const TC_PIV_application_id application = inventory->link.application;
  for (size_t i = 0; i < inventory->count && list.result == TC_PIV_OK; ++i) {
    const TC_PIV_object* object = &inventory->objects[i];
    if (!object->info || object->state != TC_PIV_OBJECT_PRESENT)
      continue;
    if (object->info->kind == TC_PIV_KIND_CERTIFICATE)
      container_add(&list, object,
                    application == TC_PIV_APPLICATION_TWIC ? TC_PIV_CERTIFICATE_TWIC
                                                           : TC_PIV_CERTIFICATE_SLOT);
    else if (object->info->kind == TC_PIV_KIND_SM_SIGNER)
      container_add(&list, object, TC_PIV_CERTIFICATE_SM_SIGNER);
    else
      cms_add(&list, object_cms(object, application, limits));
  }
  if (list.result != TC_PIV_OK)
    return list.result;
  *count = list.count;
  return TC_PIV_OK;
}
#endif
