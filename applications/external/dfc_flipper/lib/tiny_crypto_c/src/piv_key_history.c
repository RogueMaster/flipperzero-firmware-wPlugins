/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key History object reader. */
#include <tiny_crypto/piv_card_objects.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_card_objects_internal.h"
#include "pki_internal.h"

enum {
  ON_CARD_TAG = 0xc1,
  OFF_CARD_TAG = 0xc2,
  URL_TAG = 0xf3,
  ERROR_DETECTION_TAG = 0xfe,
  HASH_DIGITS = 64, /* ASCII-HEX SHA-256 of OffCardKeyHistoryFile */
  LABEL_MAX = 63
};

static const char url_scheme[] = "http://";
#define URL_SCHEME_BYTES (sizeof url_scheme - 1)

static int ldh(uint8_t c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
}

static int hex_digit(uint8_t c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Dot-separated LDH labels of 1 to 63 bytes, each without a leading or
 * trailing hyphen (RFC 5890 section 2.3.1). */
static int dns_name(const uint8_t* name, size_t length)
{
  size_t label = 0;
  for (size_t i = 0; i < length; ++i) {
    if (name[i] == '.') {
      if (!label || name[i - 1] == '-')
        return 0;
      label = 0;
    } else if (!ldh(name[i]) || (!label && name[i] == '-') || ++label > LABEL_MAX)
      return 0;
  }
  return label && name[length - 1] != '-';
}

/* Part 1 section 3.3.3: "http://" <DNS name> "/" <ASCII-HEX SHA-256>. */
static int url_valid(TC_bytes url)
{
  if (url.length > TC_PIV_KEY_HISTORY_URL_MAX_BYTES ||
      url.length < URL_SCHEME_BYTES + 1 + HASH_DIGITS ||
      memcmp(url.data, url_scheme, URL_SCHEME_BYTES) != 0)
    return 0;
  const size_t host_length = url.length - URL_SCHEME_BYTES - 1 - HASH_DIGITS;
  const uint8_t* host = url.data + URL_SCHEME_BYTES;
  const uint8_t* hash = host + host_length + 1;
  if (!dns_name(host, host_length) || host[host_length] != '/')
    return 0;
  for (size_t i = 0; i < HASH_DIGITS; ++i)
    if (!hex_digit(hash[i]))
      return 0;
  return 1;
}

/* One unsigned binary count byte (Part 1 Table 20 footnote 24). */
static TC_TLV_result count_read(TC_TLV_reader* reader, unsigned tag, uint8_t* count)
{
  TC_TLV_element element;
  TC_TLV_result result = tc_pki_field(reader, tag, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.value.length != 1)
    return TC_TLV_INVALID;
  *count = element.value.data[0];
  return TC_TLV_OK;
}

static TC_TLV_result key_history_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                      TC_PIV_key_history* out)
{
  TC_PIV_key_history parsed = {0, 0, {NULL, 0}};
  TC_TLV_element element;
  TC_TLV_reader reader = {0};
  TC_bytes contents;
  TC_TLV_result result = tc_piv_object_contents(encoded, encoding, out, sizeof *out, &contents);
  if (result == TC_TLV_OK)
    result = TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &tc_piv_object_limits);
  if (result == TC_TLV_OK)
    result = count_read(&reader, ON_CARD_TAG, &parsed.on_card);
  if (result == TC_TLV_OK)
    result = count_read(&reader, OFF_CARD_TAG, &parsed.off_card);
  if (result != TC_TLV_OK)
    return result;
  /* Section 3.3.3: at most 20 retired key management keys. */
  if ((unsigned)parsed.on_card + parsed.off_card > TC_PIV_KEY_HISTORY_MAX_KEYS)
    return TC_TLV_INVALID;
  if (reader.offset < contents.length && contents.data[reader.offset] == URL_TAG) {
    result = tc_pki_field(&reader, URL_TAG, &element);
    if (result != TC_TLV_OK)
      return result;
    if (!url_valid(element.value))
      return TC_TLV_INVALID;
    parsed.url = element.value;
  }
  result = tc_pki_field(&reader, ERROR_DETECTION_TAG, &element);
  if (result != TC_TLV_OK)
    return result;
  if (element.value.length || !tc_pki_end(&reader))
    return TC_TLV_INVALID;
  /* Table 20 footnote 25: the URL is required when off_card is nonzero and
   * absent when both counts are zero. */
  const int has_url = parsed.url.data != NULL;
  if ((parsed.off_card && !has_url) || (!parsed.on_card && !parsed.off_card && has_url))
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_key_history_read(TC_bytes encoded, TC_PIV_container_encoding encoding,
                                      TC_PIV_key_history* out)
{
  return tc_piv_object_result(key_history_read(encoded, encoding, out));
}
#endif
