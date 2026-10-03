/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Name constraints and name-chaining checks (RFC 5280 sections 4.2.1.10 and
 * 6.1.3): directoryName, dNSName, rfc822Name, SmtpUTF8Mailbox (RFC 9598),
 * URI and iPAddress subtree matching, issuer-subject chaining and the
 * per-certificate subject and subjectAltName constraint check. */
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

static int hex_value(uint8_t c)
{
  c = tc_ascii_fold(c);
  if (c >= '0' && c <= '9')
    return c - '0';
  return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int domain_next(TC_bytes name, size_t* offset, int escaped)
{
  int c = name.data[(*offset)++];
  if (escaped && c == '%') {
    int high, low;
    if (name.length - *offset < 2)
      return -1;
    high = hex_value(name.data[*offset]);
    low = hex_value(name.data[*offset + 1]);
    if (high < 0 || low < 0)
      return -1;
    *offset += 2;
    c = (high << 4) | low;
    /* Only unreserved URI characters can be normalized before comparison. */
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '.' || c == '_' || c == '~'))
      return -1;
  }
  return tc_ascii_fold((uint8_t)c);
}

static int uri_ipv4(TC_bytes host, int escaped);

static TC_TLV_result dns_syntax(TC_bytes name, size_t* work, int escaped, size_t* length)
{
  size_t i = 0, label = 0, count = 0;
  int previous = 0;
  if (!name.length)
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(work, name.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  while (i < name.length) {
    const int c = domain_next(name, &i, escaped);
    if (c < 0 || ++count > 253)
      return TC_TLV_INVALID;
    if (c == '*')
      return TC_TLV_UNSUPPORTED;
    if (c == '.') {
      if (!label || previous == '-')
        return TC_TLV_INVALID;
      label = 0;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
        return TC_TLV_INVALID;
      if ((!label && c == '-') || ++label > 63)
        return TC_TLV_INVALID;
    }
    previous = c;
  }
  if (!label || previous == '-')
    return TC_TLV_INVALID;
  *length = count;
  return TC_TLV_OK;
}

static TC_TLV_result domain_within(TC_bytes name, TC_bytes base, size_t* work, int* matched,
                                   int descendants, int escaped)
{
  TC_TLV_result result;
  size_t offset, i, position = 0, length, base_length;
  int previous = 0;
  int strict = base.length && base.data[0] == '.';
  if (strict) {
    ++base.data;
    --base.length;
  }
  result = dns_syntax(name, work, escaped, &length);
  if (result != TC_TLV_OK)
    return result;
  result = dns_syntax(base, work, 0, &base_length);
  if (result != TC_TLV_OK)
    return result;
  if (escaped && uri_ipv4(name, 1))
    return TC_TLV_INVALID;
  if (length < base_length || (strict && length == base_length) ||
      (!strict && !descendants && length != base_length)) {
    *matched = 0;
    return TC_TLV_OK;
  }
  offset = length - base_length;
  if (tc_pki_work_charge(work, name.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  for (i = 0; i < offset; ++i)
    previous = domain_next(name, &position, escaped);
  if (offset && previous != '.') {
    *matched = 0;
    return TC_TLV_OK;
  }
  for (i = 0; i < base_length; ++i)
    if (domain_next(name, &position, escaped) != tc_ascii_fold(base.data[i])) {
      *matched = 0;
      return TC_TLV_OK;
    }
  *matched = 1;
  return TC_TLV_OK;
}

static int mail_atom(uint8_t c)
{
  c = tc_ascii_fold(c);
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
    return 1;
  switch (c) {
  case '!':
  case '#':
  case '$':
  case '%':
  case '&':
  case '\'':
  case '*':
  case '+':
  case '-':
  case '/':
  case '=':
  case '?':
  case '^':
  case '_':
  case '`':
  case '{':
  case '|':
  case '}':
  case '~':
    return 1;
  default:
    return 0;
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
  if (!name.length)
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(work, name.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  /* A quoted local part may contain @, including a backslash-quoted one. */
  if (name.data[0] == '"') {
    i = 1;
    while (i < name.length && name.data[i] != '"') {
      if (name.data[i] >= 128) {
        if (mail_nonascii(name, &i, utf8) != TC_TLV_OK)
          return TC_TLV_INVALID;
        nonascii = 1;
        continue;
      }
      uint8_t c = name.data[i++];
      if (c == '\\') {
        if (i == name.length)
          return TC_TLV_INVALID;
        c = name.data[i++];
      }
      if (c < 32 || c > 126)
        return TC_TLV_INVALID;
    }
    if (i == name.length)
      return TC_TLV_INVALID;
    ++i;
  } else {
    size_t atom = 0;
    while (i < name.length && name.data[i] != '@') {
      if (name.data[i] >= 128) {
        if (mail_nonascii(name, &i, utf8) != TC_TLV_OK)
          return TC_TLV_INVALID;
        nonascii = 1;
        ++atom;
        continue;
      }
      uint8_t c = name.data[i++];
      if (c == '.') {
        if (!atom)
          return TC_TLV_INVALID;
        atom = 0;
      } else {
        if (!mail_atom(c))
          return TC_TLV_INVALID;
        ++atom;
      }
    }
    if (!atom)
      return TC_TLV_INVALID;
  }
  if (i > 64 || i == name.length || name.data[i] != '@' || ++i == name.length)
    return TC_TLV_INVALID;
  if (name.data[i] == '[')
    return TC_TLV_UNSUPPORTED;
  if (utf8 && !nonascii)
    return TC_TLV_INVALID;
  domain->data = name.data + i;
  domain->length = name.length - i;
  if (utf8) {
    size_t label = i;
    for (; i < name.length; ++i) {
      uint8_t c = name.data[i];
      if (c >= 'A' && c <= 'Z')
        return TC_TLV_INVALID;
      if (c == '.')
        label = i + 1;
      if (i - label == 3 && c == '-' && name.data[i - 1] == '-' &&
          (name.data[label] != 'x' || name.data[label + 1] != 'n'))
        return TC_TLV_INVALID;
    }
  }
  return TC_TLV_OK;
}

static TC_TLV_result mail_within(TC_bytes name, TC_bytes base, size_t* work, int* matched, int utf8)
{
  TC_bytes domain;
  TC_TLV_result result;
  size_t i;
  if (tc_pki_work_charge(work, base.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  /* RFC 9549 removed mailbox-specific constraints. Do not widen them to hosts. */
  for (i = 0; i < base.length; ++i)
    if (base.data[i] == '@')
      return TC_TLV_UNSUPPORTED;
  result = mail_domain(name, &domain, work, utf8);
  if (result != TC_TLV_OK)
    return result;
  return domain_within(domain, base, work, matched, 0, 0);
}

static TC_TLV_result smtp_utf8(TC_bytes contents, const TC_TLV_limits* limits, size_t* work,
                               TC_bytes* mailbox)
{
  static const uint8_t smtp_oid[] = {0x2b, 6, 1, 5, 5, 7, 8, 9};
  TC_TLV_reader reader;
  TC_TLV_element oid, wrapper, value;
  TC_TLV_result result;
  if (limits->max_elements < 3 || limits->max_depth < 2)
    return TC_TLV_LIMIT;
  if (tc_pki_work_charge(work, contents.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_TLV_reader_init(&reader, contents, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_next(&reader, 6, &oid);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (TC_DER_oid_contents(oid.value) != TC_TLV_OK)
    return TC_TLV_INVALID;
  result = tc_pki_next(&reader, 0xa0, &wrapper);
  if (result != TC_TLV_OK)
    return result == TC_TLV_END ? TC_TLV_INVALID : result;
  if (!tc_pki_end(&reader))
    return TC_TLV_INVALID;
  result = TC_TLV_read(wrapper.value, TC_TLV_DER, limits, &value);
  if (result != TC_TLV_OK)
    return result;
  if (value.encoded.length != wrapper.value.length)
    return TC_TLV_INVALID;
  if (oid.value.length != sizeof smtp_oid || memcmp(oid.value.data, smtp_oid, sizeof smtp_oid))
    return TC_TLV_END;
  if (!tc_pki_tag(&value, 0x0c))
    return TC_TLV_INVALID;
  *mailbox = value.value;
  return TC_TLV_OK;
}

static int uri_char(uint8_t c)
{
  c = tc_ascii_fold(c);
  if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
    return 1;
  switch (c) {
  case '-':
  case '.':
  case '_':
  case '~':
  case '!':
  case '$':
  case '&':
  case '\'':
  case '(':
  case ')':
  case '*':
  case '+':
  case ',':
  case ';':
  case '=':
    return 1;
  default:
    return 0;
  }
}

static int uri_hex(uint8_t c)
{
  return hex_value(c) >= 0;
}

static int uri_ipv4(TC_bytes host, int escaped)
{
  size_t i = 0;
  unsigned part = 0, digits = 0, value = 0;
  while (i < host.length) {
    int c = domain_next(host, &i, escaped);
    if (c == '.') {
      if (!digits || ++part > 3)
        return 0;
      digits = value = 0;
    } else {
      if (c < '0' || c > '9' || digits == 3 || (digits && !value))
        return 0;
      value = value * 10 + (unsigned)(c - '0');
      ++digits;
      if (value > 255)
        return 0;
    }
  }
  return part == 3 && digits != 0;
}

static TC_TLV_result uri_domain(TC_bytes name, TC_bytes* domain, size_t* work)
{
  size_t i, start, end, host, port;
  unsigned section = 0;
  int userinfo = 0;
  if (!name.length)
    return TC_TLV_INVALID;
  if (tc_pki_work_charge(work, name.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (tc_ascii_fold(name.data[0]) < 'a' || tc_ascii_fold(name.data[0]) > 'z')
    return TC_TLV_INVALID;
  for (i = 1; i < name.length && name.data[i] != ':'; ++i) {
    uint8_t c = tc_ascii_fold(name.data[i]);
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'))
      return TC_TLV_INVALID;
  }
  /* RFC 5280 rejects constrained URIs without a DNS authority host. */
  if (name.length - i < 3 || name.data[i + 1] != '/' || name.data[i + 2] != '/')
    return TC_TLV_INVALID;
  start = i + 3;
  for (end = start; end < name.length; ++end)
    if (name.data[end] == '/' || name.data[end] == '?' || name.data[end] == '#')
      break;
  host = start;
  for (i = start; i < end; ++i) {
    if (name.data[i] == '@') {
      if (userinfo)
        return TC_TLV_INVALID;
      userinfo = 1;
      host = i + 1;
    }
  }
  for (i = start; userinfo && i < host - 1; ++i) {
    uint8_t c = name.data[i];
    if (c == '%') {
      if (host - 1 - i < 3 || !uri_hex(name.data[i + 1]) || !uri_hex(name.data[i + 2]))
        return TC_TLV_INVALID;
      i += 2;
    } else if (!uri_char(c) && c != ':')
      return TC_TLV_INVALID;
  }
  if (host == end || name.data[host] == '[')
    return TC_TLV_INVALID;
  for (port = host; port < end && name.data[port] != ':'; ++port) {
  }
  domain->data = name.data + host;
  domain->length = port - host;
  for (i = port; i < end; ++i)
    if (i != port && (name.data[i] < '0' || name.data[i] > '9'))
      return TC_TLV_INVALID;
  /* Check the ignored components too, so malformed escapes cannot hide there. */
  for (i = end; i < name.length; ++i) {
    uint8_t c = name.data[i];
    if (c == '#' && section < 2) {
      section = 2;
      continue;
    }
    if (c == '?' && section == 0) {
      section = 1;
      continue;
    }
    if (c == '%') {
      if (name.length - i < 3 || !uri_hex(name.data[i + 1]) || !uri_hex(name.data[i + 2]))
        return TC_TLV_INVALID;
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
  if (result != TC_TLV_OK)
    return result;
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
  if (tc_pki_work_charge(work, width) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  for (i = 0; i < width; ++i) {
    const uint8_t mask = base.data[width + i];
    unsigned bit;
    for (bit = 128; bit; bit >>= 1) {
      if (mask & bit) {
        if (zero)
          return TC_TLV_INVALID;
      } else
        zero = 1;
    }
    if (name.length == width)
      mismatch |= (name.data[i] ^ base.data[i]) & mask;
  }
  *matched = name.length == width && !mismatch;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_general_name_within(const TC_X509_general_name* name,
                                          const TC_X509_general_subtree* subtree,
                                          const TC_TLV_limits* limits,
                                          const TC_X509_name_workspace* workspace, size_t* work,
                                          int* matched)
{
  TC_bytes inputs[4];
  size_t i;
  if (!name || !subtree || !limits || !work || !matched || name->type > 8 ||
      subtree->base.type > 8 || (!name->value.data && name->value.length) ||
      (!subtree->base.value.data && subtree->base.value.length))
    return TC_TLV_ARGUMENT;
  inputs[0] = name->value;
  inputs[1] = subtree->base.value;
  inputs[2].data = (const uint8_t*)name;
  inputs[2].length = sizeof(*name);
  inputs[3].data = (const uint8_t*)subtree;
  inputs[3].length = sizeof(*subtree);
  if (!tc_internal_ranges_disjoint(work, sizeof(*work), matched, sizeof(*matched)) ||
      !tc_internal_ranges_disjoint(work, sizeof(*work), limits, sizeof(*limits)) ||
      !tc_internal_ranges_disjoint(matched, sizeof(*matched), limits, sizeof(*limits)))
    return TC_TLV_ARGUMENT;
  for (i = 0; i < 4; ++i)
    if (!tc_internal_ranges_disjoint(work, sizeof(*work), inputs[i].data, inputs[i].length) ||
        !tc_internal_ranges_disjoint(matched, sizeof(*matched), inputs[i].data, inputs[i].length))
      return TC_TLV_ARGUMENT;
  if (name->value.length > limits->max_input || subtree->base.value.length > limits->max_input ||
      name->value.length > limits->max_value || subtree->base.value.length > limits->max_value)
    return TC_TLV_LIMIT;
  if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (name->type == 0 && subtree->base.type == 1) {
    TC_bytes mailbox;
    TC_TLV_result result = smtp_utf8(name->value, limits, work, &mailbox);
    if (result == TC_TLV_END) {
      *matched = 0;
      return TC_TLV_OK;
    }
    if (result != TC_TLV_OK)
      return result;
    if (subtree->minimum || subtree->has_maximum)
      return TC_TLV_UNSUPPORTED;
    return mail_within(mailbox, subtree->base.value, work, matched, 1);
  }
  if (name->type != subtree->base.type) {
    *matched = 0;
    return TC_TLV_OK;
  }
  if (subtree->minimum || subtree->has_maximum)
    return TC_TLV_UNSUPPORTED;
  switch (name->type) {
  case 1:
    return mail_within(name->value, subtree->base.value, work, matched, 0);
  case 2:
    return domain_within(name->value, subtree->base.value, work, matched, 1, 0);
  case 4:
    return TC_X509_name_within(name->value, subtree->base.value, limits, workspace, work, matched);
  case 6:
    return uri_within(name->value, subtree->base.value, work, matched);
  case 7:
    return ip_within(name->value, subtree->base.value, work, matched);
  default:
    return TC_TLV_UNSUPPORTED;
  }
}
static TC_TLV_result constraint_storage(const TC_bytes* inputs, size_t input_count,
                                        const TC_TLV_limits* limits,
                                        const TC_X509_constraint_workspace* workspace, size_t* work,
                                        int* permitted)
{
  TC_bytes reads[3], writes[6];
  size_t i, j, count = 3;
  if (!limits || !workspace || !work || !permitted ||
      workspace->frames.capacity > SIZE_MAX / sizeof(TC_TLV_frame) ||
      (!workspace->frames.data && workspace->frames.capacity))
    return TC_TLV_ARGUMENT;
  reads[0].data = (const uint8_t*)limits;
  reads[0].length = sizeof(*limits);
  reads[1].data = (const uint8_t*)workspace;
  reads[1].length = sizeof(*workspace);
  reads[2].data = (const uint8_t*)workspace->names;
  reads[2].length = workspace->names ? sizeof(*workspace->names) : 0;
  writes[0].data = (const uint8_t*)workspace->frames.data;
  writes[0].length = workspace->frames.capacity * sizeof(TC_TLV_frame);
  writes[1].data = (const uint8_t*)work;
  writes[1].length = sizeof(*work);
  writes[2].data = (const uint8_t*)permitted;
  writes[2].length = sizeof(*permitted);
  if (workspace->names) {
    const TC_X509_name_workspace* names = workspace->names;
    if (names->scalar_capacity > SIZE_MAX / sizeof(uint32_t))
      return TC_TLV_ARGUMENT;
    writes[3].data = (const uint8_t*)names->left;
    writes[3].length = names->scalar_capacity * sizeof(uint32_t);
    writes[4].data = (const uint8_t*)names->right;
    writes[4].length = writes[3].length;
    writes[5].data = names->matched;
    writes[5].length = names->attribute_capacity;
    count = 6;
  }
  for (i = 0; i < input_count; ++i)
    if (!inputs[i].data && inputs[i].length)
      return TC_TLV_ARGUMENT;
  for (i = 0; i < count; ++i) {
    if (!writes[i].data && writes[i].length)
      return TC_TLV_ARGUMENT;
    for (j = 0; j < 3; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, reads[j].data,
                                       reads[j].length))
        return TC_TLV_ARGUMENT;
    for (j = 0; j < input_count; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, inputs[j].data,
                                       inputs[j].length))
        return TC_TLV_ARGUMENT;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, writes[j].data,
                                       writes[j].length))
        return TC_TLV_ARGUMENT;
  }
  return TC_TLV_OK;
}

TC_X509_signature_result TC_X509_issuer_check(const TC_X509_certificate* certificate,
                                              TC_bytes issuer_name,
                                              const TC_X509_public_key* issuer_key,
                                              const TC_X509_signature_provider* provider,
                                              const TC_TLV_limits* limits,
                                              const TC_X509_name_workspace* workspace, size_t* work)
{
  TC_bytes inputs[16];
  TC_X509_constraint_workspace scratch = {{NULL, 0}, workspace};
  TC_TLV_result result;
  int equal;
  if (!certificate || !issuer_key || !workspace)
    return TC_X509_SIGNATURE_ERROR;
  if (!provider || !provider->verify)
    return TC_X509_SIGNATURE_UNSUPPORTED;
  inputs[0] = certificate->encoded;
  inputs[1] = certificate->issuer;
  inputs[2] = issuer_name;
  inputs[3] = certificate->tbs;
  inputs[4] = certificate->signature;
  inputs[5] = certificate->signature_algorithm.oid;
  inputs[6] = certificate->signature_algorithm.parameters;
  inputs[7] = issuer_key->key;
  inputs[8] = issuer_key->algorithm.oid;
  inputs[9] = issuer_key->algorithm.parameters;
  inputs[10] = issuer_key->curve_oid;
  inputs[11].data = (const uint8_t*)certificate;
  inputs[11].length = sizeof(*certificate);
  inputs[12].data = (const uint8_t*)issuer_key;
  inputs[12].length = sizeof(*issuer_key);
  inputs[13].data = (const uint8_t*)provider;
  inputs[13].length = sizeof(*provider);
  inputs[14] = issuer_key->modulus;
  inputs[15] = issuer_key->exponent;
  result = constraint_storage(inputs, 16, limits, &scratch, work, &equal);
  if (result != TC_TLV_OK)
    return TC_X509_SIGNATURE_ERROR;
  result = TC_X509_name_equal(certificate->issuer, issuer_name, limits, workspace, work, &equal);
  if (result != TC_TLV_OK)
    return tc_pki_signature_error(result);
  if (!equal)
    return TC_X509_SIGNATURE_INVALID;
  return TC_X509_signature_verify(certificate, issuer_key, provider, work);
}

TC_TLV_result TC_X509_name_constraints_check(const TC_X509_general_name* name,
                                             const TC_X509_name_constraints* constraints,
                                             const TC_TLV_limits* limits,
                                             const TC_X509_constraint_workspace* workspace,
                                             size_t* work, int* permitted)
{
  TC_bytes inputs[5], lists[2];
  TC_TLV_limits budget;
  TC_X509_general_subtrees_reader reader;
  TC_X509_general_subtree subtree;
  TC_TLV_result result;
  unsigned form, list;
  int restricted = 0, included = 0, excluded = 0;
  if (!name || !constraints || name->type > 8)
    return TC_TLV_ARGUMENT;
  lists[0] = constraints->permitted;
  lists[1] = constraints->excluded;
  inputs[0] = name->value;
  inputs[1] = lists[0];
  inputs[2] = lists[1];
  inputs[3].data = (const uint8_t*)name;
  inputs[3].length = sizeof(*name);
  inputs[4].data = (const uint8_t*)constraints;
  inputs[4].length = sizeof(*constraints);
  result = constraint_storage(inputs, 5, limits, workspace, work, permitted);
  if (result != TC_TLV_OK)
    return result;
  if (name->value.length > limits->max_input || name->value.length > limits->max_value ||
      lists[0].length > limits->max_input || lists[1].length > limits->max_input - lists[0].length)
    return TC_TLV_LIMIT;
  if (tc_pki_work_charge(work, lists[0].length) != TC_TLV_OK ||
      tc_pki_work_charge(work, lists[1].length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  form = name->type;
  if (!form && (lists[0].length || lists[1].length)) {
    TC_bytes mailbox;
    result = smtp_utf8(name->value, limits, work, &mailbox);
    if (result == TC_TLV_OK)
      form = 1;
    else if (result != TC_TLV_END)
      return result;
  }
  budget = *limits;
  for (list = 0; list < 2; ++list) {
    result = TC_X509_general_subtrees_init(&reader, lists[list], &budget, workspace->frames);
    if (result != TC_TLV_OK)
      return result;
    while ((result = TC_X509_general_subtree_next(&reader, &subtree)) == TC_TLV_OK) {
      int matched;
      if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (subtree.base.type != form && subtree.base.type != name->type)
        continue;
      result =
          TC_X509_general_name_within(name, &subtree, limits, workspace->names, work, &matched);
      if (result != TC_TLV_OK)
        return result;
      if (!list) {
        restricted = 1;
        included |= matched;
      } else
        excluded |= matched;
    }
    if (result != TC_TLV_END)
      return result;
    budget.max_elements -= reader.reader.elements;
  }
  *permitted = (!restricted || included) && !excluded;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_certificate_names_check_san(const TC_X509_certificate* certificate,
                                                  TC_bytes san,
                                                  const TC_X509_name_constraints* constraints,
                                                  const TC_TLV_limits* limits,
                                                  const TC_X509_constraint_workspace* workspace,
                                                  size_t* work, int* permitted)
{
  TC_bytes inputs[7];
  TC_TLV_reader reader, subject;
  TC_X509_general_name name = {0, {NULL, 0}, {NULL, 0}};
  TC_TLV_result result;
  const int has_san = san.data != NULL;
  int allowed = 1, current;
  if (!certificate || !constraints)
    return TC_TLV_ARGUMENT;
  inputs[0] = certificate->encoded;
  inputs[1] = certificate->subject;
  inputs[2] = certificate->extensions;
  inputs[3] = constraints->permitted;
  inputs[4] = constraints->excluded;
  inputs[5].data = (const uint8_t*)certificate;
  inputs[5].length = sizeof(*certificate);
  inputs[6].data = (const uint8_t*)constraints;
  inputs[6].length = sizeof(*constraints);
  result = constraint_storage(inputs, 7, limits, workspace, work, permitted);
  if (result != TC_TLV_OK)
    return result;
  if (certificate->encoded.length > limits->max_input)
    return TC_TLV_LIMIT;
  result = tc_x509_name_validate(certificate->subject, limits, work, &subject);
  if (result != TC_TLV_OK)
    return result;
  if (subject.input.length) {
    name.type = 4;
    name.value = certificate->subject;
    result = TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
    if (result != TC_TLV_OK)
      return result;
    allowed &= current;
  } else if (!has_san)
    return TC_TLV_INVALID;
  if (has_san) {
    if (tc_pki_work_charge(work, san.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    TC_X509_general_names_reader names;
    result = TC_X509_general_names_init(&names, san, limits, workspace->frames);
    if (result != TC_TLV_OK)
      return result;
    while ((result = TC_X509_general_name_next(&names, &name)) == TC_TLV_OK) {
      if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      result =
          TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
      if (result != TC_TLV_OK)
        return result;
      allowed &= current;
    }
  } else {
    TC_bytes rdn;
    /* RFC 5280 applies legacy subject email constraints only without SAN. */
    while ((result = TC_X509_rdn_next(&subject, &rdn)) == TC_TLV_OK) {
      TC_X509_name_attribute attribute;
      result = TC_TLV_reader_init(&reader, rdn, TC_TLV_DER, limits);
      if (result != TC_TLV_OK)
        return result;
      while ((result = tc_x509_name_next_attribute(&reader, work, &attribute, NULL)) == TC_TLV_OK) {
        if (tc_x509_attribute_syntax(attribute.oid) == TC_X509_ATTRIBUTE_EMAIL) {
          TC_TLV_element value;
          result = TC_TLV_read(attribute.value, TC_TLV_DER, limits, &value);
          if (result != TC_TLV_OK)
            return result;
          if (!tc_pki_tag(&value, 0x16))
            return TC_TLV_INVALID;
          name.type = 1;
          name.value = value.value;
          result =
              TC_X509_name_constraints_check(&name, constraints, limits, workspace, work, &current);
          if (result != TC_TLV_OK)
            return result;
          allowed &= current;
        }
      }
      if (result != TC_TLV_END)
        return result;
    }
  }
  if (result != TC_TLV_END)
    return result;
  *permitted = allowed;
  return TC_TLV_OK;
}

TC_TLV_result TC_X509_certificate_names_check(const TC_X509_certificate* certificate,
                                              const TC_X509_name_constraints* constraints,
                                              const TC_TLV_limits* limits,
                                              const TC_X509_constraint_workspace* workspace,
                                              size_t* work, int* permitted)
{
  TC_bytes san = {NULL, 0};
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  if (!certificate || !constraints || !limits || !work)
    return TC_TLV_ARGUMENT;
  result = tc_pki_extensions_init(&reader, certificate, limits, work);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    if (tc_pki_extension_id(&extension) == TC_PKI_EXT_SUBJECT_ALT_NAME) {
      if (san.data)
        return TC_TLV_INVALID;
      san = extension.value;
    }
  }
  if (result != TC_TLV_END)
    return result;
  return tc_x509_certificate_names_check_san(certificate, san, constraints, limits, workspace, work,
                                             permitted);
}
#endif
