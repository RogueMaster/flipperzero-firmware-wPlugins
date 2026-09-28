/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509
#include "internal.h"
#include "pki_internal.h"
#include "pki_tree_internal.h"
#include "pki_extensions_internal.h"
#include "pki_names_internal.h"
#include "pki_status_internal.h"
#include "unicode_internal.h"
#include "string_internal.h"

static TC_TLV_result charge(size_t* work, size_t amount)
{
  if (*work < amount) { *work = 0; return TC_TLV_LIMIT; }
  *work -= amount;
  return TC_TLV_OK;
}

static uint8_t ascii_fold(uint8_t c)
{ return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c; }

static TC_TLV_result validate_name(TC_bytes input, const TC_TLV_limits* limits,
                                  size_t* work, TC_TLV_reader* reader)
{
  TC_TLV_reader check;
  TC_bytes rdn;
  TC_TLV_result result;
  /* Reserve both traversals: schema validation and RDN matching. */
  if (charge(work, input.length) != TC_TLV_OK || charge(work, input.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_X509_name_init(reader, input, limits);
  if (result != TC_TLV_OK) return result;
  check = *reader;
  while ((result = TC_X509_rdn_next(&check, &rdn)) == TC_TLV_OK) {}
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

static TC_TLV_result next_attribute(TC_TLV_reader* reader, size_t* work,
    TC_X509_name_attribute* attribute, const tc_pki_tree_workspace* tree)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (tree) return tc_pki_tree_attribute(reader,tree,attribute);
  if (reader->offset == reader->input.length) return TC_TLV_END;
  if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_read(reader->input.data + reader->offset,
      reader->input.length - reader->offset, TC_TLV_DER, &reader->limits, &element);
  if (result != TC_TLV_OK) return result;
  if (charge(work, element.encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  return TC_X509_attribute_next(reader, attribute);
}

static int matching_rule(TC_bytes oid)
{
  switch (tc_x509_attribute_syntax(oid)) {
    case TC_X509_ATTRIBUTE_DIRECTORY:
    case TC_X509_ATTRIBUTE_PRINTABLE:
    case TC_X509_ATTRIBUTE_COUNTRY: return 1;
    case TC_X509_ATTRIBUTE_DOMAIN: return 2;
    default: return 0;
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
  TC_TLV_result result = charge(state->work,1);
  if (result != TC_TLV_OK) return result;
  if (state->rule == 1)
    return tc_unicode_prepare_point(point,state->buffer,state->capacity,&state->used,state->work);
  if (point > 127) return TC_TLV_INVALID;
  if (state->used == state->capacity) return TC_TLV_LIMIT;
  state->buffer[state->used++] = ascii_fold((uint8_t)point);
  return TC_TLV_OK;
}

static TC_TLV_result prepare(const TC_X509_name_attribute* attribute,
    TC_TLV_profile profile, uint32_t* buffer, size_t capacity, size_t* written, size_t* work,
    const TC_TLV_limits* bounds, const tc_pki_tree_workspace* tree)
{
  const TC_TLV_limits limits = {SIZE_MAX,SIZE_MAX,1,1};
  TC_TLV_element value;
  TC_TLV_result result;
  int rule = matching_rule(attribute->oid);
  size_t i;
  if (!rule) return TC_TLV_UNSUPPORTED;
  result = tree ? tc_pki_tree_read(attribute->value,profile,bounds,tree,&value) :
      TC_TLV_read(attribute->value.data, attribute->value.length, profile, &limits, &value);
  if (result != TC_TLV_OK) return result;
  if (value.header.tag_length != 1) return TC_TLV_UNSUPPORTED;
  if (value.header.constructed) {
    name_preparation state = {rule,buffer,capacity,0,work};
    size_t bytes;
    if (!tree) return TC_TLV_INVALID;
    result = tc_pki_string_walk(attribute->value,value.header.tag[0] & ~0x20u,profile,
        bounds,tree->frames,tree->capacity,work,prepare_point,&state,&bytes);
    if (result != TC_TLV_OK) return result;
    if (rule == 1) return tc_unicode_prepare_finish(buffer,state.used,capacity,written,work);
    *written = state.used;
    return TC_TLV_OK;
  }
  if (rule == 1) {
    const unsigned tag = value.header.tag[0];
    if (tag != 0x0c && tag != 0x13 && tag != 0x1c && tag != 0x1e) return TC_TLV_UNSUPPORTED;
    return tc_unicode_prepare(tag, value.value, buffer, capacity, written, work);
  }
  if (value.header.tag[0] != 0x16) return TC_TLV_INVALID;
  if (value.value.length > capacity) return TC_TLV_LIMIT;
  if (charge(work, value.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  for (i = 0; i < value.value.length; ++i) {
    uint8_t c = value.value.data[i];
    if (c > 127) return TC_TLV_INVALID;
    buffer[i] = ascii_fold(c);
  }
  *written = value.value.length;
  return TC_TLV_OK;
}

static TC_TLV_result rdn_equal(TC_bytes left, TC_bytes right,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* workspace,
    size_t* work, int* equal, TC_TLV_profile left_profile, TC_TLV_profile right_profile,
    const tc_pki_tree_workspace* tree)
{
  TC_TLV_reader a, b;
  TC_X509_name_attribute first, second;
  TC_TLV_result result;
  size_t count = 0, seen = 0;
  result = TC_TLV_reader_init(&b, right.data, right.length, right_profile, limits);
  if (result != TC_TLV_OK) return result;
  while ((result = next_attribute(&b, work, &second, tree)) == TC_TLV_OK) {
    if (count == workspace->attribute_capacity) return TC_TLV_LIMIT;
    workspace->matched[count++] = 0;
  }
  if (result != TC_TLV_END) return result;
  result = TC_TLV_reader_init(&a, left.data, left.length, left_profile, limits);
  if (result != TC_TLV_OK) return result;
  while ((result = next_attribute(&a, work, &first, tree)) == TC_TLV_OK) {
    size_t length, index = 0;
    int found = 0;
    if (++seen > count) { *equal = 0; return TC_TLV_OK; }
    result = prepare(&first, left_profile, workspace->left, workspace->scalar_capacity,
        &length, work, limits, tree);
    if (result != TC_TLV_OK) return result;
    result = TC_TLV_reader_init(&b, right.data, right.length, right_profile, limits);
    if (result != TC_TLV_OK) return result;
    while ((result = next_attribute(&b, work, &second, tree)) == TC_TLV_OK) {
      if (!workspace->matched[index] && tc_pki_equal(first.oid, second.oid)) {
        size_t other_length;
        result = prepare(&second, right_profile, workspace->right, workspace->scalar_capacity,
            &other_length, work, limits, tree);
        if (result != TC_TLV_OK) return result;
        if (charge(work, length) != TC_TLV_OK) return TC_TLV_LIMIT;
        if (length == other_length && !memcmp(workspace->left, workspace->right, length * sizeof(uint32_t))) {
          workspace->matched[index] = 1;
          found = 1;
          break;
        }
      }
      ++index;
    }
    if (result != TC_TLV_OK && result != TC_TLV_END) return result;
    if (!found) { *equal = 0; return TC_TLV_OK; }
  }
  if (result != TC_TLV_END) return result;
  *equal = seen == count;
  return TC_TLV_OK;
}

static TC_TLV_result open_name(TC_bytes input, TC_TLV_profile profile,
    const TC_TLV_limits* limits, size_t* work, const tc_pki_tree_workspace* tree,
    TC_TLV_reader* reader)
{
  TC_TLV_result result;
  if (!tree) return validate_name(input,limits,work,reader);
  result = tc_pki_tree_name(input,profile,limits,tree);
  if (result != TC_TLV_OK) return result;
  return tc_pki_tree_open(input,0x30,profile,limits,tree,reader);
}

static TC_TLV_result next_rdn(TC_TLV_reader* reader,
    const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  TC_TLV_element element;
  TC_TLV_result result;
  if (!tree) return TC_X509_rdn_next(reader,out);
  result = tc_pki_tree_next(reader,tree,&element);
  if (result != TC_TLV_OK) return result;
  if (!tc_pki_tag(&element,0x31) || !element.value.length) return TC_TLV_INVALID;
  *out = element.value;
  return TC_TLV_OK;
}

static TC_TLV_result next_name_rdn(TC_TLV_reader* reader, TC_bytes* suffix,
    const tc_pki_tree_workspace* tree, TC_bytes* out)
{
  TC_TLV_result result = next_rdn(reader,tree,out);
  if (result != TC_TLV_END || !suffix->length) return result;
  *out = *suffix;
  *suffix = (TC_bytes){NULL,0};
  return TC_TLV_OK;
}

static TC_TLV_result match(TC_bytes left, TC_bytes right,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* workspace,
    size_t* work, int* matched, int subtree, TC_TLV_profile left_profile,
    TC_TLV_profile right_profile, const tc_pki_tree_workspace* tree,
    TC_bytes left_rdn, TC_bytes right_rdn)
{
  TC_TLV_reader a, b;
  TC_bytes rdn_a, rdn_b;
  TC_TLV_result result;
  size_t i, j;
  int equal;
  if (!limits || !workspace || !work || !matched || (!left.data && left.length)
      || (!right.data && right.length) || workspace->scalar_capacity > SIZE_MAX / sizeof(uint32_t)
      || ((!workspace->left || !workspace->right) && workspace->scalar_capacity)
      || (!workspace->matched && workspace->attribute_capacity)) return TC_TLV_ARGUMENT;
  if ((!left_rdn.data && left_rdn.length) || (!right_rdn.data && right_rdn.length) ||
      ((left_rdn.length || right_rdn.length) && (!tree || left_profile != TC_TLV_DER ||
          right_profile != TC_TLV_DER))) return TC_TLV_ARGUMENT;
  if ((left_profile != TC_TLV_DER && left_profile != TC_TLV_BER) ||
      (right_profile != TC_TLV_DER && right_profile != TC_TLV_BER) ||
      (tree && (tree->work != work || tree->capacity > SIZE_MAX / sizeof(TC_TLV_frame) ||
          (!tree->frames && tree->capacity)))) return TC_TLV_ARGUMENT;
  const TC_bytes writes[] = {
    {(const uint8_t*)workspace->left,workspace->scalar_capacity * sizeof(uint32_t)},
    {(const uint8_t*)workspace->right,workspace->scalar_capacity * sizeof(uint32_t)},
    {workspace->matched,workspace->attribute_capacity},
    {(const uint8_t*)work,sizeof(*work)},
    {(const uint8_t*)matched,sizeof(*matched)},
    {(const uint8_t*)(tree ? tree->frames : NULL),tree ? tree->capacity * sizeof(TC_TLV_frame) : 0}
  };
  for (i = 0; i < sizeof writes / sizeof writes[0]; ++i) {
    if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, left.data, left.length)
        || !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, right.data, right.length)
        || !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, left_rdn.data, left_rdn.length)
        || !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, right_rdn.data, right_rdn.length)
        || !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, limits, sizeof(*limits))
        || !tc_internal_ranges_disjoint(writes[i].data, writes[i].length, workspace, sizeof(*workspace))
        || (tree && !tc_internal_ranges_disjoint(writes[i].data,writes[i].length,tree,sizeof(*tree))))
      return TC_TLV_ARGUMENT;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, writes[j].data, writes[j].length))
        return TC_TLV_ARGUMENT;
  }
  if (left_rdn.length) {
    result = tc_pki_rdn_contents_check(left_rdn,limits,tree);
    if (result != TC_TLV_OK) return result;
  }
  if (right_rdn.length) {
    result = tc_pki_rdn_contents_check(right_rdn,limits,tree);
    if (result != TC_TLV_OK) return result;
  }
  result = open_name(left, left_profile, limits, work, tree, &a);
  if (result != TC_TLV_OK) return result;
  result = open_name(right, right_profile, limits, work, tree, &b);
  if (result != TC_TLV_OK) return result;
  for (;;) {
    result = next_name_rdn(&b,&right_rdn,tree,&rdn_b);
    if (result == TC_TLV_END) { *matched = subtree || (tc_pki_end(&a) && !left_rdn.length); return TC_TLV_OK; }
    if (result != TC_TLV_OK) return result;
    result = next_name_rdn(&a,&left_rdn,tree,&rdn_a);
    if (result == TC_TLV_END) { *matched = 0; return TC_TLV_OK; }
    if (result != TC_TLV_OK) return result;
    result = rdn_equal(rdn_a, rdn_b, limits, workspace, work, &equal,
        left_profile,right_profile,tree);
    if (result != TC_TLV_OK) return result;
    if (!equal) { *matched = 0; return TC_TLV_OK; }
  }
}

TC_TLV_result TC_X509_name_equal(TC_bytes left, TC_bytes right,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* workspace,
    size_t* work, int* matched)
{ return match(left, right, limits, workspace, work, matched, 0,TC_TLV_DER,TC_TLV_DER,NULL,
    (TC_bytes){NULL,0},(TC_bytes){NULL,0}); }

TC_TLV_result TC_X509_name_within(TC_bytes name, TC_bytes subtree,
    const TC_TLV_limits* limits, const TC_X509_name_workspace* workspace,
    size_t* work, int* matched)
{ return match(name, subtree, limits, workspace, work, matched, 1,TC_TLV_DER,TC_TLV_DER,NULL,
    (TC_bytes){NULL,0},(TC_bytes){NULL,0}); }

TC_TLV_result tc_pki_name_equal(TC_bytes left, TC_TLV_profile left_profile,
    TC_bytes right, TC_TLV_profile right_profile, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, const tc_pki_tree_workspace* tree,
    int* matched)
{
  if (!tree) return TC_TLV_ARGUMENT;
  return match(left,right,limits,workspace,tree->work,matched,0,left_profile,right_profile,tree,
      (TC_bytes){NULL,0},(TC_bytes){NULL,0});
}

TC_TLV_result tc_pki_name_appended_equal(TC_bytes left, TC_bytes left_rdn,
    TC_bytes right, TC_bytes right_rdn, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, const tc_pki_tree_workspace* tree,
    int* matched)
{
  if (!tree) return TC_TLV_ARGUMENT;
  return match(left,right,limits,workspace,tree->work,matched,0,TC_TLV_DER,TC_TLV_DER,tree,
      left_rdn,right_rdn);
}

static int hex_value(uint8_t c)
{
  c = ascii_fold(c);
  if (c >= '0' && c <= '9') return c - '0';
  return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int domain_next(TC_bytes name, size_t* offset, int escaped)
{
  int c = name.data[(*offset)++];
  if (escaped && c == '%') {
    int high, low;
    if (name.length - *offset < 2) return -1;
    high = hex_value(name.data[*offset]); low = hex_value(name.data[*offset + 1]);
    if (high < 0 || low < 0) return -1;
    *offset += 2; c = (high << 4) | low;
    /* Only unreserved URI characters can be normalized before comparison. */
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
          || c == '-' || c == '.' || c == '_' || c == '~')) return -1;
  }
  return ascii_fold((uint8_t)c);
}

static int uri_ipv4(TC_bytes host, int escaped);

static TC_TLV_result dns_syntax(TC_bytes name, size_t* work, int escaped, size_t* length)
{
  size_t i = 0, label = 0, count = 0;
  int previous = 0;
  if (!name.length) return TC_TLV_INVALID;
  if (charge(work, name.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  while (i < name.length) {
    const int c = domain_next(name, &i, escaped);
    if (c < 0 || ++count > 253) return TC_TLV_INVALID;
    if (c == '*') return TC_TLV_UNSUPPORTED;
    if (c == '.') {
      if (!label || previous == '-') return TC_TLV_INVALID;
      label = 0;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return TC_TLV_INVALID;
      if ((!label && c == '-') || ++label > 63) return TC_TLV_INVALID;
    }
    previous = c;
  }
  if (!label || previous == '-') return TC_TLV_INVALID;
  *length = count;
  return TC_TLV_OK;
}

static TC_TLV_result domain_within(TC_bytes name, TC_bytes base,
    size_t* work, int* matched, int descendants, int escaped)
{
  TC_TLV_result result;
  size_t offset, i, position = 0, length, base_length;
  int previous = 0;
  int strict = base.length && base.data[0] == '.';
  if (strict) { ++base.data; --base.length; }
  result = dns_syntax(name, work, escaped, &length);
  if (result != TC_TLV_OK) return result;
  result = dns_syntax(base, work, 0, &base_length);
  if (result != TC_TLV_OK) return result;
  if (escaped && uri_ipv4(name, 1)) return TC_TLV_INVALID;
  if (length < base_length || (strict && length == base_length)
      || (!strict && !descendants && length != base_length)) {
    *matched = 0; return TC_TLV_OK;
  }
  offset = length - base_length;
  if (charge(work, name.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  for (i = 0; i < offset; ++i) previous = domain_next(name, &position, escaped);
  if (offset && previous != '.') { *matched = 0; return TC_TLV_OK; }
  for (i = 0; i < base_length; ++i)
    if (domain_next(name, &position, escaped) != ascii_fold(base.data[i])) {
      *matched = 0; return TC_TLV_OK;
    }
  *matched = 1;
  return TC_TLV_OK;
}

static int mail_atom(uint8_t c)
{
  c = ascii_fold(c);
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 1;
  switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'':
    case '*': case '+': case '-': case '/': case '=': case '?':
    case '^': case '_': case '`': case '{': case '|': case '}': case '~': return 1;
    default: return 0;
  }
}

static TC_TLV_result mail_nonascii(TC_bytes name, size_t* offset, int utf8)
{
  uint32_t point;
  if (!utf8 || tc_asn1_string_next(0x0c, name, offset, &point) != TC_TLV_OK || point == 0xfeff)
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

static TC_TLV_result mail_domain(TC_bytes name, TC_bytes* domain, size_t* work, int utf8)
{
  size_t i = 0;
  int nonascii = 0;
  if (!name.length) return TC_TLV_INVALID;
  if (charge(work, name.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  /* A quoted local part may contain @, including a backslash-quoted one. */
  if (name.data[0] == '"') {
    i = 1;
    while (i < name.length && name.data[i] != '"') {
      if (name.data[i] >= 128) {
        if (mail_nonascii(name, &i, utf8) != TC_TLV_OK) return TC_TLV_INVALID;
        nonascii = 1; continue;
      }
      uint8_t c = name.data[i++];
      if (c == '\\') {
        if (i == name.length) return TC_TLV_INVALID;
        c = name.data[i++];
      }
      if (c < 32 || c > 126) return TC_TLV_INVALID;
    }
    if (i == name.length) return TC_TLV_INVALID;
    ++i;
  } else {
    size_t atom = 0;
    while (i < name.length && name.data[i] != '@') {
      if (name.data[i] >= 128) {
        if (mail_nonascii(name, &i, utf8) != TC_TLV_OK) return TC_TLV_INVALID;
        nonascii = 1; ++atom; continue;
      }
      uint8_t c = name.data[i++];
      if (c == '.') {
        if (!atom) return TC_TLV_INVALID;
        atom = 0;
      } else {
        if (!mail_atom(c)) return TC_TLV_INVALID;
        ++atom;
      }
    }
    if (!atom) return TC_TLV_INVALID;
  }
  if (i > 64 || i == name.length || name.data[i] != '@' || ++i == name.length)
    return TC_TLV_INVALID;
  if (name.data[i] == '[') return TC_TLV_UNSUPPORTED;
  if (utf8 && !nonascii) return TC_TLV_INVALID;
  domain->data = name.data + i; domain->length = name.length - i;
  if (utf8) {
    size_t label = i;
    for (; i < name.length; ++i) {
      uint8_t c = name.data[i];
      if (c >= 'A' && c <= 'Z') return TC_TLV_INVALID;
      if (c == '.') label = i + 1;
      if (i - label == 3 && c == '-' && name.data[i - 1] == '-'
          && (name.data[label] != 'x' || name.data[label + 1] != 'n')) return TC_TLV_INVALID;
    }
  }
  return TC_TLV_OK;
}

static TC_TLV_result mail_within(TC_bytes name, TC_bytes base, size_t* work, int* matched, int utf8)
{
  TC_bytes domain;
  TC_TLV_result result;
  size_t i;
  if (charge(work, base.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  /* RFC 9549 removed mailbox-specific constraints. Do not widen them to hosts. */
  for (i = 0; i < base.length; ++i)
    if (base.data[i] == '@') return TC_TLV_UNSUPPORTED;
  result = mail_domain(name, &domain, work, utf8);
  if (result != TC_TLV_OK) return result;
  return domain_within(domain, base, work, matched, 0, 0);
}

static TC_TLV_result smtp_utf8(TC_bytes contents, const TC_TLV_limits* limits,
    size_t* work, TC_bytes* mailbox)
{
  static const uint8_t smtp_oid[] = {0x2b,6,1,5,5,7,8,9};
  TC_TLV_reader reader;
  TC_TLV_element oid, wrapper, value;
  TC_TLV_result result;
  if (limits->max_elements < 3 || limits->max_depth < 2) return TC_TLV_LIMIT;
  if (charge(work, contents.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader, contents.data, contents.length, TC_TLV_DER, limits);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_next(&reader, 6, &oid);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (TC_DER_oid_contents(oid.value.data, oid.value.length) != TC_TLV_OK) return TC_TLV_INVALID;
  result = tc_pki_next(&reader, 0xa0, &wrapper);
  if (result != TC_TLV_OK) return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!tc_pki_end(&reader)) return TC_TLV_INVALID;
  result = TC_TLV_read(wrapper.value.data, wrapper.value.length, TC_TLV_DER, limits, &value);
  if (result != TC_TLV_OK) return result;
  if (value.encoded.length != wrapper.value.length) return TC_TLV_INVALID;
  if (oid.value.length != sizeof smtp_oid || memcmp(oid.value.data, smtp_oid, sizeof smtp_oid)) return TC_TLV_END;
  if (!tc_pki_tag(&value, 0x0c)) return TC_TLV_INVALID;
  *mailbox = value.value;
  return TC_TLV_OK;
}

static int uri_char(uint8_t c)
{
  c = ascii_fold(c);
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return 1;
  switch (c) {
    case '-': case '.': case '_': case '~':
    case '!': case '$': case '&': case '\'': case '(': case ')':
    case '*': case '+': case ',': case ';': case '=': return 1;
    default: return 0;
  }
}

static int uri_hex(uint8_t c)
{ return hex_value(c) >= 0; }

static int uri_ipv4(TC_bytes host, int escaped)
{
  size_t i = 0;
  unsigned part = 0, digits = 0, value = 0;
  while (i < host.length) {
    int c = domain_next(host, &i, escaped);
    if (c == '.') {
      if (!digits || ++part > 3) return 0;
      digits = value = 0;
    } else {
      if (c < '0' || c > '9' || digits == 3 || (digits && !value)) return 0;
      value = value * 10 + (unsigned)(c - '0'); ++digits;
      if (value > 255) return 0;
    }
  }
  return part == 3 && digits != 0;
}

static TC_TLV_result uri_domain(TC_bytes name, TC_bytes* domain, size_t* work)
{
  size_t i, start, end, host, port;
  unsigned section = 0;
  int userinfo = 0;
  if (!name.length) return TC_TLV_INVALID;
  if (charge(work, name.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (ascii_fold(name.data[0]) < 'a' || ascii_fold(name.data[0]) > 'z') return TC_TLV_INVALID;
  for (i = 1; i < name.length && name.data[i] != ':'; ++i) {
    uint8_t c = ascii_fold(name.data[i]);
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'))
      return TC_TLV_INVALID;
  }
  /* RFC 5280 rejects constrained URIs without a DNS authority host. */
  if (name.length - i < 3 || name.data[i + 1] != '/' || name.data[i + 2] != '/') return TC_TLV_INVALID;
  start = i + 3;
  for (end = start; end < name.length; ++end)
    if (name.data[end] == '/' || name.data[end] == '?' || name.data[end] == '#') break;
  host = start;
  for (i = start; i < end; ++i) {
    if (name.data[i] == '@') {
      if (userinfo) return TC_TLV_INVALID;
      userinfo = 1; host = i + 1;
    }
  }
  for (i = start; userinfo && i < host - 1; ++i) {
    uint8_t c = name.data[i];
    if (c == '%') {
      if (host - 1 - i < 3 || !uri_hex(name.data[i + 1]) || !uri_hex(name.data[i + 2])) return TC_TLV_INVALID;
      i += 2;
    } else if (!uri_char(c) && c != ':') return TC_TLV_INVALID;
  }
  if (host == end || name.data[host] == '[') return TC_TLV_INVALID;
  for (port = host; port < end && name.data[port] != ':'; ++port) {}
  domain->data = name.data + host; domain->length = port - host;
  for (i = port; i < end; ++i)
    if (i != port && (name.data[i] < '0' || name.data[i] > '9')) return TC_TLV_INVALID;
  /* Check the ignored components too, so malformed escapes cannot hide there. */
  for (i = end; i < name.length; ++i) {
    uint8_t c = name.data[i];
    if (c == '#' && section < 2) { section = 2; continue; }
    if (c == '?' && section == 0) { section = 1; continue; }
    if (c == '%') {
      if (name.length - i < 3 || !uri_hex(name.data[i + 1]) || !uri_hex(name.data[i + 2])) return TC_TLV_INVALID;
      i += 2;
    } else if (!uri_char(c) && c != ':' && c != '@' && c != '/' && !(c == '?' && section))
      return TC_TLV_INVALID;
  }
  return TC_TLV_OK;
}

static TC_TLV_result uri_within(TC_bytes name, TC_bytes base, size_t* work, int* matched)
{
  TC_bytes domain;
  TC_TLV_result result = uri_domain(name, &domain, work);
  if (result != TC_TLV_OK) return result;
  return domain_within(domain, base, work, matched, 0, 1);
}

static TC_TLV_result ip_within(TC_bytes name, TC_bytes base, size_t* work, int* matched)
{
  size_t width, i;
  unsigned mismatch = 0;
  int zero = 0;
  if ((name.length != 4 && name.length != 16) || (base.length != 8 && base.length != 32))
    return TC_TLV_INVALID;
  width = base.length / 2;
  if (charge(work, width) != TC_TLV_OK) return TC_TLV_LIMIT;
  for (i = 0; i < width; ++i) {
    const uint8_t mask = base.data[width + i];
    unsigned bit;
    for (bit = 128; bit; bit >>= 1) {
      if (mask & bit) { if (zero) return TC_TLV_INVALID; }
      else zero = 1;
    }
    if (name.length == width) mismatch |= (name.data[i] ^ base.data[i]) & mask;
  }
  *matched = name.length == width && !mismatch;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_name_within(const TC_X509_general_name* name,
    const TC_X509_general_subtree* subtree, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, size_t* work, int* matched)
{
  TC_bytes inputs[4];
  size_t i;
  if (!name || !subtree || !limits || !work || !matched || name->type > 8 || subtree->base.type > 8
      || (!name->value.data && name->value.length) || (!subtree->base.value.data && subtree->base.value.length))
    return TC_TLV_ARGUMENT;
  inputs[0] = name->value; inputs[1] = subtree->base.value;
  inputs[2].data = (const uint8_t*)name; inputs[2].length = sizeof(*name);
  inputs[3].data = (const uint8_t*)subtree; inputs[3].length = sizeof(*subtree);
  if (!tc_internal_ranges_disjoint(work, sizeof(*work), matched, sizeof(*matched))
      || !tc_internal_ranges_disjoint(work, sizeof(*work), limits, sizeof(*limits))
      || !tc_internal_ranges_disjoint(matched, sizeof(*matched), limits, sizeof(*limits))) return TC_TLV_ARGUMENT;
  for (i = 0; i < 4; ++i)
    if (!tc_internal_ranges_disjoint(work, sizeof(*work), inputs[i].data, inputs[i].length)
        || !tc_internal_ranges_disjoint(matched, sizeof(*matched), inputs[i].data, inputs[i].length))
      return TC_TLV_ARGUMENT;
  if (name->value.length > limits->max_input || subtree->base.value.length > limits->max_input
      || name->value.length > limits->max_value || subtree->base.value.length > limits->max_value)
    return TC_TLV_LIMIT;
  if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
  if (name->type == 0 && subtree->base.type == 1) {
    TC_bytes mailbox;
    TC_TLV_result result = smtp_utf8(name->value, limits, work, &mailbox);
    if (result == TC_TLV_END) { *matched = 0; return TC_TLV_OK; }
    if (result != TC_TLV_OK) return result;
    if (subtree->minimum || subtree->has_maximum) return TC_TLV_UNSUPPORTED;
    return mail_within(mailbox, subtree->base.value, work, matched, 1);
  }
  if (name->type != subtree->base.type) { *matched = 0; return TC_TLV_OK; }
  if (subtree->minimum || subtree->has_maximum) return TC_TLV_UNSUPPORTED;
  switch (name->type) {
    case 1: return mail_within(name->value, subtree->base.value, work, matched, 0);
    case 2: return domain_within(name->value, subtree->base.value, work, matched, 1, 0);
    case 4: return TC_X509_name_within(name->value, subtree->base.value, limits, workspace, work, matched);
    case 6: return uri_within(name->value, subtree->base.value, work, matched);
    case 7: return ip_within(name->value, subtree->base.value, work, matched);
    default: return TC_TLV_UNSUPPORTED;
  }
}
static TC_TLV_result constraint_storage(const TC_bytes* inputs, size_t input_count,
    const TC_TLV_limits* limits,
    const TC_X509_constraint_workspace* workspace, size_t* work, int* permitted)
{
  TC_bytes reads[3], writes[6];
  size_t i, j, count = 3;
  if (!limits || !workspace || !work || !permitted
      || workspace->frame_capacity > SIZE_MAX / sizeof(TC_TLV_frame)
      || (!workspace->frames && workspace->frame_capacity)) return TC_TLV_ARGUMENT;
  reads[0].data = (const uint8_t*)limits; reads[0].length = sizeof(*limits);
  reads[1].data = (const uint8_t*)workspace; reads[1].length = sizeof(*workspace);
  reads[2].data = (const uint8_t*)workspace->names; reads[2].length = workspace->names ? sizeof(*workspace->names) : 0;
  writes[0].data = (const uint8_t*)workspace->frames; writes[0].length = workspace->frame_capacity * sizeof(TC_TLV_frame);
  writes[1].data = (const uint8_t*)work; writes[1].length = sizeof(*work);
  writes[2].data = (const uint8_t*)permitted; writes[2].length = sizeof(*permitted);
  if (workspace->names) {
    const TC_X509_name_workspace* names = workspace->names;
    if (names->scalar_capacity > SIZE_MAX / sizeof(uint32_t)) return TC_TLV_ARGUMENT;
    writes[3].data = (const uint8_t*)names->left; writes[3].length = names->scalar_capacity * sizeof(uint32_t);
    writes[4].data = (const uint8_t*)names->right; writes[4].length = writes[3].length;
    writes[5].data = names->matched; writes[5].length = names->attribute_capacity;
    count = 6;
  }
  for (i = 0; i < input_count; ++i)
    if (!inputs[i].data && inputs[i].length) return TC_TLV_ARGUMENT;
  for (i = 0; i < count; ++i) {
    if (!writes[i].data && writes[i].length) return TC_TLV_ARGUMENT;
    for (j = 0; j < 3; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, reads[j].data, reads[j].length))
        return TC_TLV_ARGUMENT;
    for (j = 0; j < input_count; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, inputs[j].data, inputs[j].length))
        return TC_TLV_ARGUMENT;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, writes[j].data, writes[j].length))
        return TC_TLV_ARGUMENT;
  }
  return TC_TLV_OK;
}

TC_X509_signature_result TC_X509_issuer_check(const TC_X509_certificate* certificate,
    TC_bytes issuer_name, const TC_X509_public_key* issuer_key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_X509_name_workspace* workspace, size_t* work)
{
  TC_bytes inputs[16];
  TC_X509_constraint_workspace scratch = {NULL,0,workspace};
  TC_TLV_result result;
  int equal;
  if (!certificate || !issuer_key || !workspace) return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify) return TC_X509_SIGNATURE_UNSUPPORTED;
  inputs[0] = certificate->encoded; inputs[1] = certificate->issuer; inputs[2] = issuer_name;
  inputs[3] = certificate->tbs; inputs[4] = certificate->signature;
  inputs[5] = certificate->signature_algorithm.oid; inputs[6] = certificate->signature_algorithm.parameters;
  inputs[7] = issuer_key->key; inputs[8] = issuer_key->algorithm.oid; inputs[9] = issuer_key->algorithm.parameters;
  inputs[10] = issuer_key->curve_oid;
  inputs[11].data = (const uint8_t*)certificate; inputs[11].length = sizeof(*certificate);
  inputs[12].data = (const uint8_t*)issuer_key; inputs[12].length = sizeof(*issuer_key);
  inputs[13].data = (const uint8_t*)provider; inputs[13].length = sizeof(*provider);
  inputs[14] = issuer_key->modulus; inputs[15] = issuer_key->exponent;
  result = constraint_storage(inputs, 16, limits, &scratch, work, &equal);
  if (result != TC_TLV_OK) return TC_X509_SIGNATURE_ERROR;
  result = TC_X509_name_equal(certificate->issuer, issuer_name, limits, workspace, work, &equal);
  if (result != TC_TLV_OK) return tc_pki_signature_error(result);
  if (!equal) return TC_X509_SIGNATURE_INVALID;
  return TC_X509_signature_verify(certificate, issuer_key, provider, work);
}

TC_TLV_result TC_X509_name_constraints_check(const TC_X509_general_name* name,
    const TC_X509_name_constraints* constraints, const TC_TLV_limits* limits,
    const TC_X509_constraint_workspace* workspace, size_t* work, int* permitted)
{
  TC_bytes inputs[5], lists[2];
  TC_TLV_limits budget;
  TC_TLV_reader reader;
  TC_X509_general_subtree subtree;
  TC_TLV_result result;
  unsigned form, list;
  int restricted = 0, included = 0, excluded = 0;
  if (!name || !constraints || name->type > 8) return TC_TLV_ARGUMENT;
  lists[0] = constraints->permitted; lists[1] = constraints->excluded;
  inputs[0] = name->value; inputs[1] = lists[0]; inputs[2] = lists[1];
  inputs[3].data = (const uint8_t*)name; inputs[3].length = sizeof(*name);
  inputs[4].data = (const uint8_t*)constraints; inputs[4].length = sizeof(*constraints);
  result = constraint_storage(inputs, 5, limits, workspace, work, permitted);
  if (result != TC_TLV_OK) return result;
  if (name->value.length > limits->max_input || name->value.length > limits->max_value
      || lists[0].length > limits->max_input || lists[1].length > limits->max_input - lists[0].length)
    return TC_TLV_LIMIT;
  if (charge(work, lists[0].length) != TC_TLV_OK || charge(work, lists[1].length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  form = name->type;
  if (!form && (lists[0].length || lists[1].length)) {
    TC_bytes mailbox;
    result = smtp_utf8(name->value, limits, work, &mailbox);
    if (result == TC_TLV_OK) form = 1;
    else if (result != TC_TLV_END) return result;
  }
  budget = *limits;
  for (list = 0; list < 2; ++list) {
    result = TC_TLV_reader_init(&reader, lists[list].data, lists[list].length, TC_TLV_DER, &budget);
    if (result != TC_TLV_OK) return result;
    while ((result = TC_X509_general_subtree_next(&reader, workspace->frames,
        workspace->frame_capacity, &subtree)) == TC_TLV_OK) {
      int matched;
      if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      if (subtree.base.type != form && subtree.base.type != name->type) continue;
      result = TC_X509_general_name_within(name, &subtree, limits, workspace->names, work, &matched);
      if (result != TC_TLV_OK) return result;
      if (!list) { restricted = 1; included |= matched; }
      else excluded |= matched;
    }
    if (result != TC_TLV_END) return result;
    budget.max_elements -= reader.elements;
  }
  *permitted = (!restricted || included) && !excluded;
  return TC_TLV_OK;
}
TC_TLV_result TC_X509_certificate_names_check(const TC_X509_certificate* certificate,
    const TC_X509_name_constraints* constraints, const TC_TLV_limits* limits,
    const TC_X509_constraint_workspace* workspace, size_t* work, int* permitted)
{
  enum { SUBJECT_ALT_NAME = 17 };
  TC_bytes inputs[7], san = {NULL,0};
  TC_TLV_reader reader, subject;
  TC_X509_extension extension;
  TC_X509_general_name name = {0,{NULL,0},{NULL,0}};
  TC_TLV_result result;
  int allowed = 1, current, has_san = 0;
  if (!certificate || !constraints) return TC_TLV_ARGUMENT;
  inputs[0] = certificate->encoded; inputs[1] = certificate->subject; inputs[2] = certificate->extensions;
  inputs[3] = constraints->permitted; inputs[4] = constraints->excluded;
  inputs[5].data = (const uint8_t*)certificate; inputs[5].length = sizeof(*certificate);
  inputs[6].data = (const uint8_t*)constraints; inputs[6].length = sizeof(*constraints);
  result = constraint_storage(inputs, 7, limits, workspace, work, permitted);
  if (result != TC_TLV_OK) return result;
  if (certificate->encoded.length > limits->max_input) return TC_TLV_LIMIT;
  result = tc_pki_extensions_init(&reader,certificate,limits,work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader,work,&extension)) == TC_TLV_OK) {
    if (tc_pki_extension_id(&extension) == SUBJECT_ALT_NAME) {
      if (has_san) return TC_TLV_INVALID;
      has_san = 1; san = extension.value;
    }
  }
  if (result != TC_TLV_END) return result;
  result = validate_name(certificate->subject, limits, work, &subject);
  if (result != TC_TLV_OK) return result;
  if (subject.input.length) {
    name.type = 4; name.value = certificate->subject;
    result = TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
    if (result != TC_TLV_OK) return result;
    allowed &= current;
  } else if (!has_san) return TC_TLV_INVALID;
  if (has_san) {
    if (charge(work, san.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_X509_general_names_init(&reader, san.data, san.length, limits);
    if (result != TC_TLV_OK) return result;
    while ((result = TC_X509_general_name_next(&reader, workspace->frames, workspace->frame_capacity, &name)) == TC_TLV_OK) {
      if (charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      result = TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
      if (result != TC_TLV_OK) return result;
      allowed &= current;
    }
  } else {
    TC_bytes rdn;
    /* RFC 5280 applies legacy subject email constraints only without SAN. */
    while ((result = TC_X509_rdn_next(&subject, &rdn)) == TC_TLV_OK) {
      TC_X509_name_attribute attribute;
      result = TC_TLV_reader_init(&reader, rdn.data, rdn.length, TC_TLV_DER, limits);
      if (result != TC_TLV_OK) return result;
      while ((result = next_attribute(&reader, work, &attribute, NULL)) == TC_TLV_OK) {
        if (tc_x509_attribute_syntax(attribute.oid) == TC_X509_ATTRIBUTE_EMAIL) {
          TC_TLV_element value;
          result = TC_TLV_read(attribute.value.data, attribute.value.length, TC_TLV_DER, limits, &value);
          if (result != TC_TLV_OK) return result;
          if (!tc_pki_tag(&value, 0x16)) return TC_TLV_INVALID;
          name.type = 1; name.value = value.value;
          result = TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
          if (result != TC_TLV_OK) return result;
          allowed &= current;
        }
      }
      if (result != TC_TLV_END) return result;
    }
  }
  if (result != TC_TLV_END) return result;
  *permitted = allowed;
  return TC_TLV_OK;
}
#endif
