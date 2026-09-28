/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_DER
#include <tiny_crypto/der.h>

static TC_TLV_result value(const uint8_t* data, size_t length,
                           uint8_t tag, TC_bytes* out)
{
  TC_TLV_limits limits = { SIZE_MAX, SIZE_MAX, 1, 1 };
  TC_TLV_element e;
  TC_TLV_result result = TC_TLV_read(data, length, TC_TLV_DER, &limits, &e);
  if (result != TC_TLV_OK) return result;
  if (e.encoded.length != length || e.header.tag_length != 1 || e.header.tag[0] != tag)
    return TC_TLV_INVALID;
  *out = e.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_integer_contents(const uint8_t* data, size_t length)
{
  if (!data && length) return TC_TLV_ARGUMENT;
  if (!length || (length > 1 &&
      ((data[0] == 0 && !(data[1] & 128)) ||
       (data[0] == 255 && (data[1] & 128))))) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_integer(const uint8_t* data, size_t length,
                            TC_bytes* out, int* negative)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!out || !negative) return TC_TLV_ARGUMENT;
  result = value(data, length, 2, &v);
  if (result != TC_TLV_OK) return result;
  /* A sign octet is allowed only when removing it would change the sign.
   * Keep it in the returned two's-complement span; do not normalize signed data. */
  result = TC_DER_integer_contents(v.data, v.length);
  if (result != TC_TLV_OK) return result;
  *negative = (v.data[0] & 128) != 0;
  *out = v;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_uint32(const uint8_t* data, size_t length, uint32_t* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = value(data, length, 2, &v);
  return result == TC_TLV_OK ? TC_DER_uint32_contents(v.data, v.length, out) : result;
}

TC_TLV_result TC_DER_uint32_contents(const uint8_t* data, size_t length, uint32_t* out)
{
  size_t i;
  uint32_t n = 0;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = TC_DER_integer_contents(data, length);
  if (result != TC_TLV_OK) return result;
  if (data[0] & 128) return TC_TLV_INVALID;
  for (i = 0; i < length; ++i) {
    if (n > (UINT32_MAX - data[i]) / 256) return TC_TLV_LIMIT;
    n = n * 256 + data[i];
  }
  *out = n;
  return TC_TLV_OK;
}

static TC_TLV_result bit_string_contents(TC_bytes v,
                                         TC_bytes* out, unsigned* unused)
{
  unsigned n;
  if (!v.length) return TC_TLV_INVALID;
  n = v.data[0];
  if (n > 7 || (v.length == 1 && n) ||
      (n && (v.data[v.length - 1] & ((1u << n) - 1)))) return TC_TLV_INVALID;
  ++v.data; --v.length;
  *out = v; *unused = n;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_bit_string(const uint8_t* data, size_t length,
                              TC_bytes* out, unsigned* unused)
{
  TC_bytes v;
  if (!out || !unused) return TC_TLV_ARGUMENT;
  TC_TLV_result result = value(data,length,3,&v);
  return result == TC_TLV_OK ? bit_string_contents(v,out,unused) : result;
}

TC_TLV_result TC_DER_oid(const uint8_t* data, size_t length, TC_bytes* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = value(data, length, 6, &v);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_oid_contents(v.data, v.length);
  if (result != TC_TLV_OK) return result;
  *out = v;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_oid_contents(const uint8_t* data, size_t length)
{
  size_t i;
  int first = 1;
  if (!data && length) return TC_TLV_ARGUMENT;
  if (!length) return TC_TLV_INVALID;
  /* The first combined OID arc uses the same base-128 encoding as later arcs.
   * Validate the encoding without limiting arc values to a machine integer. */
  for (i = 0; i < length; ++i) {
    if (first && data[i] == 128) return TC_TLV_INVALID;
    first = !(data[i] & 128);
  }
  if (!first) return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_boolean(const uint8_t* data, size_t length, int* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = value(data, length, 1, &v);
  if (result != TC_TLV_OK) return result;
  if (v.length != 1 || (v.data[0] != 0 && v.data[0] != 255)) return TC_TLV_INVALID;
  *out = v.data[0] != 0;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_null(const uint8_t* data, size_t length)
{
  TC_bytes v;
  TC_TLV_result result = value(data, length, 5, &v);
  return result != TC_TLV_OK ? result : v.length ? TC_TLV_INVALID : TC_TLV_OK;
}
TC_TLV_result TC_DER_sequence(const uint8_t* data, size_t length, TC_bytes* out)
{ return out ? value(data, length, 0x30, out) : TC_TLV_ARGUMENT; }
TC_TLV_result TC_DER_set(const uint8_t* data, size_t length, TC_bytes* out)
{ return out ? value(data, length, 0x31, out) : TC_TLV_ARGUMENT; }

static TC_TLV_result sequence_fields(const uint8_t* data, size_t length,
    TC_bytes* fields, size_t minimum, size_t maximum)
{
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, maximum + 1, 1};
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = TC_DER_sequence(data, length, &contents);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_reader_init(&reader, contents.data, contents.length, TC_TLV_DER, &limits);
  if (result != TC_TLV_OK) return result;
  for (size_t i = 0; i < maximum; ++i) {
    result = TC_TLV_next(&reader, &element);
    if (result == TC_TLV_END && i >= minimum) {
      for (; i < maximum; ++i) fields[i] = (TC_bytes){NULL,0};
      return TC_TLV_OK;
    }
    /* The enclosing SEQUENCE is complete, so MORE is a malformed child. */
    if (result != TC_TLV_OK) return TC_TLV_INVALID;
    fields[i] = element.encoded;
  }
  return TC_TLV_next(&reader, &element) == TC_TLV_END ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result pair(const uint8_t* data, size_t length,
                          TC_bytes fields[2], int optional_second)
{
  return sequence_fields(data,length,fields,optional_second ? 1 : 2,2);
}

TC_TLV_result TC_DER_algorithm_identifier(const uint8_t* data, size_t length,
                                         TC_DER_algorithm* out)
{
  TC_bytes fields[2];
  TC_DER_algorithm algorithm;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = pair(data, length, fields, 1);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_oid(fields[0].data, fields[0].length, &algorithm.oid);
  if (result != TC_TLV_OK) return result;
  algorithm.parameters = fields[1];
  *out = algorithm;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_positive_integer(const uint8_t* data, size_t length, TC_bytes* out)
{
  TC_bytes integer;
  int negative;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_DER_integer(data, length, &integer, &negative);
  if (result != TC_TLV_OK) return result;
  if (negative || (integer.length == 1 && integer.data[0] == 0)) return TC_TLV_INVALID;
  if (integer.data[0] == 0) { ++integer.data; --integer.length; }
  *out = integer;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_rsa_public(const uint8_t* data, size_t length,
                               TC_DER_rsa_public_key* out)
{
  TC_bytes fields[2];
  TC_DER_rsa_public_key key;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = pair(data,length,fields,0);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_positive_integer(fields[0].data,fields[0].length,&key.modulus);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_positive_integer(fields[1].data,fields[1].length,&key.exponent);
  if (result != TC_TLV_OK) return result;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_rsa_private(const uint8_t* data, size_t length,
                                TC_DER_rsa_private_key* out)
{
  enum { FIELD_COUNT = 9, TWO_PRIME = 0, MULTI_PRIME = 1 };
  TC_TLV_limits limits = {SIZE_MAX,SIZE_MAX,FIELD_COUNT,1};
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element field;
  TC_DER_rsa_private_key key;
  uint32_t version;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_DER_sequence(data,length,&contents);
  if (result != TC_TLV_OK) return result;
  result = TC_TLV_reader_init(&reader,contents.data,contents.length,TC_TLV_DER,&limits);
  if (result != TC_TLV_OK) return result;
  if (TC_TLV_next(&reader,&field) != TC_TLV_OK) return TC_TLV_INVALID;
  result = TC_DER_uint32(field.encoded.data,field.encoded.length,&version);
  if (result != TC_TLV_OK) return result;
  if (version == MULTI_PRIME) return TC_TLV_UNSUPPORTED;
  if (version != TWO_PRIME) return TC_TLV_INVALID;
  TC_bytes* components[] = {&key.modulus,&key.public_exponent,&key.private_exponent,
    &key.prime1,&key.prime2,&key.exponent1,&key.exponent2,&key.coefficient};
  for (size_t i = 0; i < sizeof components / sizeof *components; ++i) {
    if (TC_TLV_next(&reader,&field) != TC_TLV_OK) return TC_TLV_INVALID;
    result = TC_DER_positive_integer(field.encoded.data,field.encoded.length,components[i]);
    if (result != TC_TLV_OK) return result;
  }
  if (reader.offset != contents.length) return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_subject_public_key(const uint8_t* data, size_t length,
                                       TC_DER_public_key* out)
{
  TC_bytes fields[2];
  TC_DER_public_key key;
  TC_TLV_result result;
  unsigned unused;
  if (!out) return TC_TLV_ARGUMENT;
  result = pair(data, length, fields, 0);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_algorithm_identifier(fields[0].data, fields[0].length, &key.algorithm);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_bit_string(fields[1].data, fields[1].length, &key.key, &unused);
  if (result != TC_TLV_OK) return result;
  if (unused || !key.key.length) return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_private_key_info(const uint8_t* data, size_t length,
                                     TC_DER_private_key* out)
{
  enum { VERSION = 0, ALGORITHM = 1, KEY = 2, OPTIONAL = 3, FIELD_COUNT = 5,
         OCTET_STRING = 0x04, IMPLICIT_ATTRIBUTES = 0xa0, IMPLICIT_PUBLIC_KEY = 0x81,
         PRIVATE_ONLY = 0, WITH_PUBLIC_KEY = 1 };
  TC_bytes fields[FIELD_COUNT];
  TC_DER_private_key key;
  uint32_t version;
  if (!out) return TC_TLV_ARGUMENT;
  TC_TLV_result result = sequence_fields(data,length,fields,3,FIELD_COUNT);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_uint32(fields[VERSION].data,fields[VERSION].length,&version);
  if (result != TC_TLV_OK) return result;
  if (version > WITH_PUBLIC_KEY) return TC_TLV_UNSUPPORTED;
  result = TC_DER_algorithm_identifier(fields[ALGORITHM].data,fields[ALGORITHM].length,
      &key.algorithm);
  if (result != TC_TLV_OK) return result;
  result = value(fields[KEY].data,fields[KEY].length,OCTET_STRING,&key.key);
  if (result != TC_TLV_OK) return result;
  key.attributes = (TC_bytes){NULL,0};
  key.public_key = (TC_bytes){NULL,0};
  key.public_key_unused = 0;
  size_t next = OPTIONAL;
  if (fields[next].data && fields[next].data[0] == IMPLICIT_ATTRIBUTES) {
    result = value(fields[next].data,fields[next].length,IMPLICIT_ATTRIBUTES,
        &key.attributes);
    if (result != TC_TLV_OK) return result;
    ++next;
  }
  if (fields[next].data) {
    TC_bytes bits;
    if (version != WITH_PUBLIC_KEY) return TC_TLV_INVALID;
    result = value(fields[next].data,fields[next].length,IMPLICIT_PUBLIC_KEY,&bits);
    if (result != TC_TLV_OK) return result;
    result = bit_string_contents(bits,&key.public_key,&key.public_key_unused);
    if (result != TC_TLV_OK) return result;
    ++next;
  } else if (version != PRIVATE_ONLY) {
    return TC_TLV_INVALID;
  }
  if (next < FIELD_COUNT && fields[next].data) return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_ecdsa_signature(const uint8_t* data, size_t length,
                                    TC_DER_signature_pair* out)
{
  TC_bytes fields[2];
  TC_DER_signature_pair signature;
  TC_TLV_result result;
  if (!out) return TC_TLV_ARGUMENT;
  result = pair(data, length, fields, 0);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_positive_integer(fields[0].data, fields[0].length, &signature.r);
  if (result != TC_TLV_OK) return result;
  result = TC_DER_positive_integer(fields[1].data, fields[1].length, &signature.s);
  if (result != TC_TLV_OK) return result;
  *out = signature;
  return TC_TLV_OK;
}
#endif
