/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * X.500 Name comparison (RFC 5280 section 7.1). RDNs match as unordered sets
 * of attributes. Directory strings are prepared with the RFC 4518 subset the
 * library supports (case folding and insignificant-space handling) before
 * comparison. domainComponent (RFC 4519) and emailAddress (RFC 2985 section
 * 5.2.1) compare IA5 values ignoring ASCII case.
 *
 * Values without a supported preparation, such as TeletexString or an
 * attribute type with no known matching rule, compare by exact encoding.
 * Identical encodings match. Different encodings are undetermined, since the
 * attribute's real matching rule could equate them. A comparison that is
 * otherwise decided only by undetermined values returns UNSUPPORTED, so
 * name-constraint and issuer checks fail closed. */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509
#include "internal.h"
#include "pki_internal.h"
#include "pki_tree_internal.h"
#include "pki_extensions_internal.h"
#include "pki_names_internal.h"
#include "pki_status_internal.h"
#include "pki_budget_internal.h"
#include "unicode_internal.h"
#include "string_internal.h"

TC_TLV_result tc_x509_name_validate(TC_bytes input, const TC_TLV_limits* limits, size_t* work,
                                    TC_TLV_reader* reader)
{
  TC_TLV_reader check;
  TC_bytes rdn;
  TC_TLV_result result;
  /* Reserve both traversals: schema validation and RDN matching. */
  if (tc_pki_work_charge(work, input.length) != TC_TLV_OK ||
      tc_pki_work_charge(work, input.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_X509_name_init(reader, input, limits);
  if (result != TC_TLV_OK)
    return result;
  check = *reader;
  while ((result = TC_X509_rdn_next(&check, &rdn)) == TC_TLV_OK) {
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

TC_TLV_result tc_x509_name_next_attribute(TC_TLV_reader* reader, size_t* work,
                                          TC_X509_name_attribute* attribute,
                                          const tc_pki_tree_workspace* tree)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (tree)
    return tc_pki_tree_attribute(reader, tree, attribute);
  if (reader->offset == reader->input.length)
    return TC_TLV_END;
  if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_TLV_read(
      (TC_bytes){reader->input.data + reader->offset, reader->input.length - reader->offset},
      TC_TLV_DER, &reader->limits, &element);
  if (result != TC_TLV_OK)
    return result;
  if (tc_pki_work_charge(work, element.encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  return TC_X509_attribute_next(reader, attribute);
}

static int matching_rule(TC_bytes oid)
{
  switch (tc_x509_attribute_syntax(oid)) {
  case TC_X509_ATTRIBUTE_DIRECTORY:
  case TC_X509_ATTRIBUTE_PRINTABLE:
  case TC_X509_ATTRIBUTE_COUNTRY:
    return 1;
  case TC_X509_ATTRIBUTE_DOMAIN:
  case TC_X509_ATTRIBUTE_EMAIL:
    return 2;
  default:
    return 0;
  }
}

typedef struct {
  int rule;
  uint32_t* buffer;
  size_t capacity, used;
  size_t* work;
} name_preparation;

static TC_TLV_result prepare_point(void* context, uint32_t point)
{
  name_preparation* state = context;
  TC_TLV_result result = tc_pki_work_charge(state->work, 1);
  if (result != TC_TLV_OK)
    return result;
  if (state->rule == 1)
    return tc_unicode_prepare_point(point, state->buffer, state->capacity, &state->used,
                                    state->work);
  if (point > 127)
    return TC_TLV_INVALID;
  if (state->used == state->capacity)
    return TC_TLV_LIMIT;
  state->buffer[state->used++] = tc_ascii_fold((uint8_t)point);
  return TC_TLV_OK;
}

static TC_TLV_result prepare(const TC_X509_name_attribute* attribute, TC_TLV_profile profile,
                             uint32_t* buffer, size_t capacity, size_t* written, size_t* work,
                             const TC_TLV_limits* bounds, const tc_pki_tree_workspace* tree)
{
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 1, 1};
  TC_TLV_element value;
  TC_TLV_result result;
  int rule = matching_rule(attribute->oid);
  size_t i;
  if (!rule)
    return TC_TLV_UNSUPPORTED;
  result = tree ? tc_pki_tree_read(attribute->value, profile, bounds, tree, &value)
                : TC_TLV_read(attribute->value, profile, &limits, &value);
  if (result != TC_TLV_OK)
    return result;
  if (value.header.tag_length != 1)
    return TC_TLV_UNSUPPORTED;
  if (value.header.constructed) {
    name_preparation state = {rule, buffer, capacity, 0, work};
    size_t bytes;
    if (!tree)
      return TC_TLV_INVALID;
    result = tc_pki_string_walk(attribute->value, value.header.tag[0] & ~0x20u, profile, bounds,
                                &(tc_pki_tree_workspace){tree->frames, tree->capacity, work},
                                prepare_point, &state, &bytes);
    if (result != TC_TLV_OK)
      return result;
    if (rule == 1)
      return tc_unicode_prepare_finish(buffer, state.used, capacity, written, work);
    *written = state.used;
    return TC_TLV_OK;
  }
  if (rule == 1) {
    const unsigned tag = value.header.tag[0];
    if (tag != 0x0c && tag != 0x13 && tag != 0x1c && tag != 0x1e)
      return TC_TLV_UNSUPPORTED;
    return tc_unicode_prepare(tag, value.value, buffer, capacity, written, work);
  }
  if (value.header.tag[0] != 0x16)
    return TC_TLV_INVALID;
  if (value.value.length > capacity)
    return TC_TLV_LIMIT;
  if (tc_pki_work_charge(work, value.value.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  for (i = 0; i < value.value.length; ++i) {
    uint8_t c = value.value.data[i];
    if (c > 127)
      return TC_TLV_INVALID;
    buffer[i] = tc_ascii_fold(c);
  }
  *written = value.value.length;
  return TC_TLV_OK;
}

/* Outcome of comparing two RDNs or two Names. */
typedef enum { NAME_MISMATCH, NAME_MATCH, NAME_UNDETERMINED } name_comparison;

/* Settings shared by every step of one Name comparison. tree is NULL for the
 * public entry points, which validate the encoding in place. */
typedef struct {
  const TC_TLV_limits* limits;
  const TC_X509_name_workspace* workspace;
  size_t* work;
  TC_TLV_profile left_profile;
  TC_TLV_profile right_profile;
  const tc_pki_tree_workspace* tree;
} name_compare;

/* Cached preparation of the left attribute across right candidates. state
 * is 0 before preparation, 1 when prepared into workspace->left and 2 when
 * only the exact encoding can be compared. */
typedef struct {
  size_t length;
  int state;
} left_preparation;

/* Compare one attribute value pair. Values that cannot be prepared compare by
 * exact encoding, where a difference is undetermined. */
static TC_TLV_result attribute_compare(const name_compare* compare,
                                       const TC_X509_name_attribute* first,
                                       const TC_X509_name_attribute* second, left_preparation* left,
                                       name_comparison* out)
{
  const TC_X509_name_workspace* workspace = compare->workspace;
  size_t* work = compare->work;
  size_t second_length = 0;
  TC_TLV_result result;
  if (!left->state) {
    result = matching_rule(first->oid) ? prepare(first, compare->left_profile, workspace->left,
                                                 workspace->scalar_capacity, &left->length, work,
                                                 compare->limits, compare->tree)
                                       : TC_TLV_UNSUPPORTED;
    if (result != TC_TLV_OK && result != TC_TLV_UNSUPPORTED)
      return result;
    left->state = result == TC_TLV_OK ? 1 : 2;
  }
  int exact = left->state == 2;
  if (!exact) {
    result = prepare(second, compare->right_profile, workspace->right, workspace->scalar_capacity,
                     &second_length, work, compare->limits, compare->tree);
    if (result == TC_TLV_UNSUPPORTED)
      exact = 1;
    else if (result != TC_TLV_OK)
      return result;
  }
  if (exact) {
    if (tc_pki_work_charge(work, first->value.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    *out = tc_pki_equal(first->value, second->value) ? NAME_MATCH : NAME_UNDETERMINED;
    return TC_TLV_OK;
  }
  if (tc_pki_work_charge(work, left->length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  *out = left->length == second_length &&
                 !memcmp(workspace->left, workspace->right, left->length * sizeof(uint32_t))
             ? NAME_MATCH
             : NAME_MISMATCH;
  return TC_TLV_OK;
}

/* RDNs match when they hold the same number of attributes and each left
 * attribute matches a distinct right attribute of the same type. A left
 * attribute with no matching or undetermined candidate decides a mismatch. */
static TC_TLV_result rdn_compare(const name_compare* compare, TC_bytes left, TC_bytes right,
                                 name_comparison* out)
{
  const TC_TLV_limits* limits = compare->limits;
  const TC_X509_name_workspace* workspace = compare->workspace;
  size_t* work = compare->work;
  const tc_pki_tree_workspace* tree = compare->tree;
  TC_TLV_reader a, b;
  TC_X509_name_attribute first, second;
  TC_TLV_result result;
  size_t count = 0, seen = 0;
  int undetermined = 0;
  result = TC_TLV_reader_init(&b, right, compare->right_profile, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_x509_name_next_attribute(&b, work, &second, tree)) == TC_TLV_OK) {
    if (count == workspace->attribute_capacity)
      return TC_TLV_LIMIT;
    workspace->matched[count++] = 0;
  }
  if (result != TC_TLV_END)
    return result;
  result = TC_TLV_reader_init(&a, left, compare->left_profile, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_x509_name_next_attribute(&a, work, &first, tree)) == TC_TLV_OK) {
    left_preparation preparation = {0, 0};
    size_t index = 0;
    int found = 0, candidate_undetermined = 0;
    if (++seen > count) {
      *out = NAME_MISMATCH;
      return TC_TLV_OK;
    }
    result = TC_TLV_reader_init(&b, right, compare->right_profile, limits);
    if (result != TC_TLV_OK)
      return result;
    while ((result = tc_x509_name_next_attribute(&b, work, &second, tree)) == TC_TLV_OK) {
      if (!workspace->matched[index] && tc_pki_equal(first.oid, second.oid)) {
        name_comparison pair;
        result = attribute_compare(compare, &first, &second, &preparation, &pair);
        if (result != TC_TLV_OK)
          return result;
        if (pair == NAME_MATCH) {
          workspace->matched[index] = 1;
          found = 1;
          break;
        }
        candidate_undetermined |= pair == NAME_UNDETERMINED;
      }
      ++index;
    }
    if (result != TC_TLV_OK && result != TC_TLV_END)
      return result;
    if (!found) {
      if (!candidate_undetermined) {
        *out = NAME_MISMATCH;
        return TC_TLV_OK;
      }
      undetermined = 1;
    }
  }
  if (result != TC_TLV_END)
    return result;
  *out = seen != count ? NAME_MISMATCH : undetermined ? NAME_UNDETERMINED : NAME_MATCH;
  return TC_TLV_OK;
}

static TC_TLV_result open_name(TC_bytes input, TC_TLV_profile profile, const TC_TLV_limits* limits,
                               size_t* work, const tc_pki_tree_workspace* tree,
                               TC_TLV_reader* reader)
{
  TC_TLV_result result;
  if (!tree)
    return tc_x509_name_validate(input, limits, work, reader);
  result = tc_pki_tree_name(input, profile, limits, tree);
  if (result != TC_TLV_OK)
    return result;
  return tc_pki_tree_open(input, 0x30, profile, limits, tree, reader);
}

static TC_TLV_result next_rdn(TC_TLV_reader* reader, const tc_pki_tree_workspace* tree,
                              TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!tree)
    return TC_X509_rdn_next(reader, out);
  result = tc_pki_tree_next(reader, tree, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_tag(&element, 0x31) || !element.value.length)
    return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

static TC_TLV_result next_name_rdn(TC_TLV_reader* reader, TC_bytes* suffix,
                                   const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  TC_TLV_result result = next_rdn(reader, tree, out);
  if (result != TC_TLV_END || !suffix->length)
    return result;
  *out = *suffix;
  *suffix = (TC_bytes){NULL, 0};
  return TC_TLV_OK;
}

/* Compare left against right. A non-empty left_rdn or right_rdn is appended
 * to its Name as a final RDN, which requires a tree and DER on both sides.
 * subtree accepts right as a leading prefix of left. */
static TC_TLV_result match(const name_compare* compare, TC_bytes left, TC_bytes left_rdn,
                           TC_bytes right, TC_bytes right_rdn, int subtree, int* matched)
{
  const TC_TLV_limits* limits = compare->limits;
  const TC_X509_name_workspace* workspace = compare->workspace;
  size_t* work = compare->work;
  const TC_TLV_profile left_profile = compare->left_profile;
  const TC_TLV_profile right_profile = compare->right_profile;
  const tc_pki_tree_workspace* tree = compare->tree;
  TC_TLV_reader a, b;
  TC_bytes rdn_a, rdn_b;
  TC_TLV_result result;
  size_t i, j;
  int undetermined = 0;
  if (!limits || !workspace || !work || !matched || (!left.data && left.length) ||
      (!right.data && right.length) || workspace->scalar_capacity > SIZE_MAX / sizeof(uint32_t) ||
      ((!workspace->left || !workspace->right) && workspace->scalar_capacity) ||
      (!workspace->matched && workspace->attribute_capacity))
    return TC_TLV_ARGUMENT;
  if ((!left_rdn.data && left_rdn.length) || (!right_rdn.data && right_rdn.length) ||
      ((left_rdn.length || right_rdn.length) &&
       (!tree || left_profile != TC_TLV_DER || right_profile != TC_TLV_DER)))
    return TC_TLV_ARGUMENT;
  if ((left_profile != TC_TLV_DER && left_profile != TC_TLV_BER) ||
      (right_profile != TC_TLV_DER && right_profile != TC_TLV_BER) ||
      (tree && (tree->work != work || tree->capacity > SIZE_MAX / sizeof(TC_TLV_frame) ||
                (!tree->frames && tree->capacity))))
    return TC_TLV_ARGUMENT;
  const TC_bytes writes[] = {
      {(const uint8_t*)workspace->left, workspace->scalar_capacity * sizeof(uint32_t)},
      {(const uint8_t*)workspace->right, workspace->scalar_capacity * sizeof(uint32_t)},
      {workspace->matched, workspace->attribute_capacity},
      {(const uint8_t*)work, sizeof(*work)},
      {(const uint8_t*)matched, sizeof(*matched)},
      {(const uint8_t*)(tree ? tree->frames : NULL),
       tree ? tree->capacity * sizeof(TC_TLV_frame) : 0}};
  for (i = 0; i < sizeof writes / sizeof writes[0]; ++i) {
    if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, left.data, left.length) ||
        !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, right.data, right.length) ||
        !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, left_rdn.data,
                                     left_rdn.length) ||
        !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, right_rdn.data,
                                     right_rdn.length) ||
        !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, limits, sizeof(*limits)) ||
        !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, workspace,
                                     sizeof(*workspace)) ||
        (tree &&
         !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, tree, sizeof(*tree))))
      return TC_TLV_ARGUMENT;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, writes[j].data,
                                       writes[j].length))
        return TC_TLV_ARGUMENT;
  }
  if (left_rdn.length) {
    result = tc_pki_rdn_contents_check(left_rdn, limits, tree);
    if (result != TC_TLV_OK)
      return result;
  }
  if (right_rdn.length) {
    result = tc_pki_rdn_contents_check(right_rdn, limits, tree);
    if (result != TC_TLV_OK)
      return result;
  }
  result = open_name(left, left_profile, limits, work, tree, &a);
  if (result != TC_TLV_OK)
    return result;
  result = open_name(right, right_profile, limits, work, tree, &b);
  if (result != TC_TLV_OK)
    return result;
  /* RDNs compare in order. A decided mismatch anywhere outranks RDNs whose
   * comparison was undetermined. */
  for (;;) {
    name_comparison rdn;
    result = next_name_rdn(&b, &right_rdn, tree, &rdn_b);
    if (result == TC_TLV_END) {
      if (!subtree && (!tc_pki_end(&a) || left_rdn.length)) {
        *matched = 0;
        return TC_TLV_OK;
      }
      if (undetermined)
        return TC_TLV_UNSUPPORTED;
      *matched = 1;
      return TC_TLV_OK;
    }
    if (result != TC_TLV_OK)
      return result;
    result = next_name_rdn(&a, &left_rdn, tree, &rdn_a);
    if (result == TC_TLV_END) {
      *matched = 0;
      return TC_TLV_OK;
    }
    if (result != TC_TLV_OK)
      return result;
    result = rdn_compare(compare, rdn_a, rdn_b, &rdn);
    if (result != TC_TLV_OK)
      return result;
    if (rdn == NAME_MISMATCH) {
      *matched = 0;
      return TC_TLV_OK;
    }
    undetermined |= rdn == NAME_UNDETERMINED;
  }
}

TC_TLV_result TC_X509_name_equal(TC_bytes left, TC_bytes right, const TC_TLV_limits* limits,
                                 const TC_X509_name_workspace* workspace, size_t* work,
                                 int* matched)
{
  const name_compare compare = {limits, workspace, work, TC_TLV_DER, TC_TLV_DER, NULL};
  return match(&compare, left, (TC_bytes){NULL, 0}, right, (TC_bytes){NULL, 0}, 0, matched);
}

TC_TLV_result TC_X509_name_within(TC_bytes name, TC_bytes subtree, const TC_TLV_limits* limits,
                                  const TC_X509_name_workspace* workspace, size_t* work,
                                  int* matched)
{
  const name_compare compare = {limits, workspace, work, TC_TLV_DER, TC_TLV_DER, NULL};
  return match(&compare, name, (TC_bytes){NULL, 0}, subtree, (TC_bytes){NULL, 0}, 1, matched);
}

TC_TLV_result tc_pki_name_equal(TC_bytes left, TC_TLV_profile left_profile, TC_bytes right,
                                TC_TLV_profile right_profile, const TC_TLV_limits* limits,
                                const TC_X509_name_workspace* workspace,
                                const tc_pki_tree_workspace* tree, int* matched)
{
  if (!tree)
    return TC_TLV_ARGUMENT;
  const name_compare compare = {limits, workspace, tree->work, left_profile, right_profile, tree};
  return match(&compare, left, (TC_bytes){NULL, 0}, right, (TC_bytes){NULL, 0}, 0, matched);
}

TC_TLV_result tc_pki_name_appended_equal(TC_bytes left, TC_bytes left_rdn, TC_bytes right,
                                         TC_bytes right_rdn, const TC_TLV_limits* limits,
                                         const TC_X509_name_workspace* workspace,
                                         const tc_pki_tree_workspace* tree, int* matched)
{
  if (!tree)
    return TC_TLV_ARGUMENT;
  const name_compare compare = {limits, workspace, tree->work, TC_TLV_DER, TC_TLV_DER, tree};
  return match(&compare, left, left_rdn, right, right_rdn, 0, matched);
}
#endif
