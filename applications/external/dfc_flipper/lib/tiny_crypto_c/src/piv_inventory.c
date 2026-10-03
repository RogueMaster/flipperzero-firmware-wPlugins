/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The catalog inventory: access-rule evaluation (SP 800-73-5 Part 1 Table 2
 * and footnote 9, TWIC Part 2 v5 4.5), one GET DATA per readable object and
 * the object states. */
#include <tiny_crypto/piv_catalog.h>
#if TC_ENABLE_PIV_CATALOG
#include "internal.h"
#include "piv_link_internal.h"

/* 1 when rule is met in the link state. OCC is never available, so its
 * alternatives reduce to the PIN. */
static int access_met(uint8_t rule, const TC_PIV_link_info* info)
{
  switch (rule) {
  case TC_PIV_ACCESS_ALWAYS:
    return 1;
  case TC_PIV_ACCESS_PIN:
  case TC_PIV_ACCESS_PIN_OR_OCC:
    return info->pin_verified;
  case TC_PIV_ACCESS_VCI:
    return info->vci;
  case TC_PIV_ACCESS_VCI_PIN:
  case TC_PIV_ACCESS_VCI_PIN_OR_OCC:
    return info->vci && info->pin_verified;
  default:
    return 0;
  }
}

/* 1 when region can take the largest single answer the card may send: Ne
 * bytes and SW1 SW2 on a plain link, one 256-byte chunk under secure
 * messaging (Part 2 4.2.6), and never more than the card's DO 7F66
 * response limit (ISO/IEC 7816-4 12.8.1). A smaller buffer could make the
 * transport fail and stop the link. */
static int region_fits(const TC_PIV_link* link, const TC_PIV_link_info* info, TC_buffer region)
{
  const uint32_t ne = info->secured ? TC_APDU_SHORT_MAX_NE : link->response_ne;
  size_t largest = (size_t)ne + TC_APDU_STATUS_BYTES;
  if (link->channel.max_response_bytes && link->channel.max_response_bytes < largest)
    largest = link->channel.max_response_bytes;
  return region.capacity >= largest;
}

/* 1 when a secure messaging session present in info ended during the last
 * command. */
static int session_lost(const TC_PIV_link* link, const TC_PIV_link_info* info)
{
  return info->secured && !(link->flags & TC_PIV_LINK_SECURED);
}

/* Map a completed GET DATA answer other than success to ABSENT or DENIED.
 * Returns 0 for a status that aborts the inventory. */
static int refusal_state(uint16_t sw, TC_PIV_application_id application, uint8_t* state)
{
  switch (TC_PIV_status_classify(sw, TC_PIV_COMMAND_GET_DATA, application, NULL)) {
  case TC_PIV_SW_NOT_FOUND:
    *state = TC_PIV_OBJECT_ABSENT;
    return 1;
  case TC_PIV_SW_SECURITY_STATUS:
  case TC_PIV_SW_NOT_SUPPORTED:
    *state = TC_PIV_OBJECT_DENIED;
    return 1;
  default:
    return 0;
  }
}

/* Read one catalog object into region and set its state. TC_PIV_OK means
 * the inventory continues. Every other result aborts it. */
static TC_PIV_result object_read(TC_PIV_link* link, const TC_PIV_link_info* info, unsigned flags,
                                 TC_buffer region, int* sent, TC_PIV_object* object)
{
  const TC_PIV_object_info* entry = object->info;
  const uint8_t rule = info->interface == TC_PIV_CONTACT ? entry->contact : entry->contactless;
  if (entry->kind == TC_PIV_KIND_PAIRING_CODE && !(flags & TC_PIV_INVENTORY_PAIRING_CODE)) {
    object->state = TC_PIV_OBJECT_SKIPPED;
    return TC_PIV_OK;
  }
  if (!access_met(rule, info)) {
    object->state = TC_PIV_OBJECT_RESTRICTED;
    return TC_PIV_OK;
  }
  if (!region_fits(link, info, region)) {
    object->state = TC_PIV_OBJECT_OVERSIZED;
    return TC_PIV_OK;
  }
  TC_PIV_data_object data;
  *sent = 1;
  const TC_PIV_result result =
      TC_PIV_get_data(link, (TC_bytes){entry->tag, entry->tag_length}, region, &data);
  object->secured = info->secured;
  switch (result) {
  case TC_PIV_OK:
    /* TWIC Part 2 v5 4.5 note: only an optional object may answer 9000
     * without data. */
    if (data.form == TC_PIV_FORM_NONE && entry->requirement != TC_PIV_OPTIONAL)
      return TC_PIV_INVALID;
    object->encoded = data.encoded;
    object->value = data.value;
    object->status = data.status;
    object->state = data.value.length ? TC_PIV_OBJECT_PRESENT : TC_PIV_OBJECT_EMPTY;
    return TC_PIV_OK;
  case TC_PIV_CARD_STATUS:
    object->status = TC_PIV_link_status(link);
    if (session_lost(link, info) ||
        !refusal_state(object->status, info->application, &object->state))
      return TC_PIV_CARD_STATUS;
    return TC_PIV_OK;
  case TC_PIV_LIMIT:
    /* The channel stays usable after a LIMIT (ISO/IEC 7816-4 5.3.4), unless
     * the budget ran out or the secure messaging session ended (Part 2
     * 4.3). */
    if (session_lost(link, info) || !TC_APDU_channel_exchanges_left(&link->channel))
      return TC_PIV_LIMIT;
    object->state = TC_PIV_OBJECT_OVERSIZED;
    return TC_PIV_OK;
  default:
    return result;
  }
}

/* Spend units of work. On a charge above the budget, empty it and return
 * 0. */
static int work_charge(size_t* work, size_t units)
{
  if (units > *work) {
    *work = 0;
    return 0;
  }
  *work -= units;
  return 1;
}

static int arguments_valid(const TC_PIV_link* link, const TC_PIV_inventory_plan* plan,
                           TC_buffer pool, const size_t* work, const TC_PIV_inventory* inventory)
{
  if (!tc_piv_link_ready(link) || !work || !inventory ||
      (plan && (plan->flags & ~(unsigned)TC_PIV_INVENTORY_PAIRING_CODE)) ||
      (!pool.data && pool.capacity) || (!inventory->objects && inventory->capacity) ||
      inventory->capacity > SIZE_MAX / sizeof *inventory->objects)
    return 0;
  const void* objects = inventory->objects;
  const size_t objects_bytes = inventory->capacity * sizeof *inventory->objects;
  /* The read writes the pool, the objects, *work and *inventory, and the
   * link changes with every command. */
  return tc_piv_link_disjoint(link, pool.data, pool.capacity) &&
         tc_piv_link_disjoint(link, objects, objects_bytes) &&
         tc_piv_link_disjoint(link, work, sizeof *work) &&
         tc_piv_link_disjoint(link, inventory, sizeof *inventory) &&
         tc_internal_ranges_disjoint(work, sizeof *work, inventory, sizeof *inventory) &&
         tc_internal_ranges_disjoint(pool.data, pool.capacity, plan, plan ? sizeof *plan : 0) &&
         tc_internal_ranges_disjoint(pool.data, pool.capacity, work, sizeof *work) &&
         tc_internal_ranges_disjoint(pool.data, pool.capacity, inventory, sizeof *inventory) &&
         tc_internal_ranges_disjoint(pool.data, pool.capacity, objects, objects_bytes) &&
         tc_internal_ranges_disjoint(objects, objects_bytes, work, sizeof *work) &&
         tc_internal_ranges_disjoint(objects, objects_bytes, inventory, sizeof *inventory);
}

/* Pool bytes one read may use: the rest of the pool, or the plan cap. */
static size_t object_cap(const TC_PIV_inventory_plan* plan, size_t pool_capacity)
{
  if (!plan || !plan->max_object_bytes || plan->max_object_bytes >= pool_capacity)
    return pool_capacity;
  return TC_PIV_RESPONSE_BYTES(plan->max_object_bytes);
}

/* Wipe the objects array and the pool bytes the card could have written,
 * and leave an empty inventory. */
static TC_PIV_result inventory_abort(TC_PIV_inventory* inventory, uint8_t* pool, size_t touched,
                                     TC_PIV_result result)
{
  TC_secure_zero(pool, touched);
  TC_secure_zero(inventory->objects, inventory->capacity * sizeof *inventory->objects);
  inventory->count = 0;
  inventory->pool = NULL;
  inventory->pool_used = 0;
  TC_secure_zero(&inventory->link, sizeof inventory->link);
  return result;
}

TC_PIV_result TC_PIV_inventory_read(TC_PIV_link* link, const TC_PIV_inventory_plan* plan,
                                    TC_buffer pool, size_t* work, TC_PIV_inventory* inventory)
{
  if (!arguments_valid(link, plan, pool, work, inventory))
    return TC_PIV_ARGUMENT;
  TC_PIV_link_info info;
  TC_PIV_link_info_get(link, &info);
  if (info.application == TC_PIV_APPLICATION_NONE || info.sm_lost)
    return TC_PIV_REFUSED;
  const size_t count = TC_PIV_catalog_count(info.application, info.profile);
  if (!count)
    return TC_PIV_UNSUPPORTED;
  if (inventory->capacity < count)
    return TC_PIV_LIMIT;
  const unsigned flags = plan ? plan->flags : 0;
  const size_t cap = object_cap(plan, pool.capacity);
  /* used: bytes kept by objects. touched: end of the last region offered. */
  size_t used = 0, touched = 0;
  for (size_t i = 0; i < count; ++i) {
    TC_PIV_object object = {TC_PIV_catalog_at(info.application, info.profile, i),
                            {NULL, 0},
                            {NULL, 0},
                            0,
                            TC_PIV_OBJECT_SKIPPED,
                            0};
    const size_t left = pool.capacity - used;
    const TC_buffer region = {pool.data ? pool.data + used : NULL, left < cap ? left : cap};
    int sent = 0;
    if (!work_charge(work, 1))
      return inventory_abort(inventory, pool.data, touched, TC_PIV_LIMIT);
    const TC_PIV_result result = object_read(link, &info, flags, region, &sent, &object);
    if (sent && used + region.capacity > touched)
      touched = used + region.capacity;
    if (result != TC_PIV_OK)
      return inventory_abort(inventory, pool.data, touched, result);
    /* The object keeps the region bytes up to its end, including the
     * secure messaging header in front of a decrypted value. */
    const size_t kept = object.encoded.length
                            ? (size_t)(object.encoded.data + object.encoded.length - region.data)
                            : 0;
    if (!work_charge(work, kept))
      return inventory_abort(inventory, pool.data, touched, TC_PIV_LIMIT);
    used += kept;
    inventory->objects[i] = object;
  }
  /* SW bytes, padding and MACs after the last object. */
  TC_secure_zero(pool.data ? pool.data + used : NULL, touched - used);
  inventory->count = count;
  inventory->pool = pool.data;
  inventory->pool_used = used;
  inventory->link = info;
  return TC_PIV_OK;
}

const TC_PIV_object* TC_PIV_inventory_find(const TC_PIV_inventory* inventory, uint16_t container)
{
  if (!inventory)
    return NULL;
  for (size_t i = 0; i < inventory->count; ++i)
    if (inventory->objects[i].info->container == container)
      return &inventory->objects[i];
  return NULL;
}

void TC_PIV_inventory_clear(TC_PIV_inventory* inventory)
{
  if (!inventory)
    return;
  TC_secure_zero(inventory->pool, inventory->pool_used);
  TC_secure_zero(inventory->objects, inventory->capacity * sizeof *inventory->objects);
  inventory->count = 0;
  inventory->pool = NULL;
  inventory->pool_used = 0;
  TC_secure_zero(&inventory->link, sizeof inventory->link);
}
#endif
