/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_DER
#include <tiny_crypto/der.h>
#include "der_bits_internal.h"
#include "internal.h"

/* Each output object is non-NULL and disjoint from the input, since writing
 * it would change the bytes that returned spans borrow. */
static int output_separate(TC_bytes encoded, const void* out, size_t size)
{
  return out && tc_internal_ranges_disjoint(out, size, encoded.data, encoded.length);
}
#define OUTPUT_SEPARATE(encoded, out) output_separate((encoded), (out), sizeof *(out))

/* Two outputs of one reader are each separate from the input and disjoint
 * from each other. */
#define OUTPUTS_SEPARATE(encoded, first, second)                                                   \
  (OUTPUT_SEPARATE(encoded, first) && OUTPUT_SEPARATE(encoded, second) &&                          \
   tc_internal_ranges_disjoint((first), sizeof *(first), (second), sizeof *(second)))

static TC_TLV_result value(TC_bytes encoded, uint8_t tag, TC_bytes* out)
{
  TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 1, 1};
  TC_TLV_element e;
  TC_TLV_result result = TC_TLV_read(encoded, TC_TLV_DER, &limits, &e);
  /* Input is the complete encoding, so a truncated object is malformed. With
   * unbounded limits, LIMIT reports a tag or length field wider than this
   * build parses, which no complete in-memory encoding needs. */
  if (result == TC_TLV_MORE || result == TC_TLV_LIMIT)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  if (e.encoded.length != encoded.length || e.header.tag_length != 1 || e.header.tag[0] != tag)
    return TC_TLV_INVALID;
  *out = e.value;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_integer_contents(TC_bytes contents)
{
  const uint8_t* data = contents.data;
  size_t length = contents.length;
  if (!data && length)
    return TC_TLV_ARGUMENT;
  /* X.690 section 8.3.2: the first nine bits are never all zero or all one. */
  if (!length ||
      (length > 1 && ((data[0] == 0 && !(data[1] & 128)) || (data[0] == 255 && (data[1] & 128)))))
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_integer(TC_bytes encoded, TC_bytes* out, int* negative)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!OUTPUTS_SEPARATE(encoded, out, negative))
    return TC_TLV_ARGUMENT;
  result = value(encoded, 2, &v);
  if (result != TC_TLV_OK)
    return result;
  /* A sign octet is allowed only when removing it would change the sign.
   * Keep it in the returned two's-complement span. Signed data stays byte-exact. */
  result = TC_DER_integer_contents(v);
  if (result != TC_TLV_OK)
    return result;
  *negative = (v.data[0] & 128) != 0;
  *out = v;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_uint32(TC_bytes encoded, uint32_t* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = value(encoded, 2, &v);
  return result == TC_TLV_OK ? TC_DER_uint32_contents(v, out) : result;
}

TC_TLV_result TC_DER_uint32_contents(TC_bytes contents, uint32_t* out)
{
  size_t i;
  uint32_t n = 0;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(contents, out))
    return TC_TLV_ARGUMENT;
  result = TC_DER_integer_contents(contents);
  if (result != TC_TLV_OK)
    return result;
  if (contents.data[0] & 128)
    return TC_TLV_INVALID;
  for (i = 0; i < contents.length; ++i) {
    if (n > (UINT32_MAX - contents.data[i]) / 256)
      return TC_TLV_LIMIT;
    n = n * 256 + contents.data[i];
  }
  *out = n;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_bit_string(TC_bytes encoded, TC_bytes* out, unsigned* unused)
{
  TC_bytes v;
  if (!OUTPUTS_SEPARATE(encoded, out, unused))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = value(encoded, 3, &v);
  return result == TC_TLV_OK ? tc_der_bit_string_contents(v, out, unused) : result;
}

TC_TLV_result TC_DER_oid(TC_bytes encoded, TC_bytes* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = value(encoded, 6, &v);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_oid_contents(v);
  if (result != TC_TLV_OK)
    return result;
  *out = v;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_oid_contents(TC_bytes contents)
{
  const uint8_t* data = contents.data;
  size_t length = contents.length;
  size_t i;
  int first = 1;
  if (!data && length)
    return TC_TLV_ARGUMENT;
  if (!length)
    return TC_TLV_INVALID;
  /* The first combined OID arc uses the same base-128 encoding as later arcs.
   * Validate the encoding. Arc values may exceed a machine integer. */
  for (i = 0; i < length; ++i) {
    if (first && data[i] == 128)
      return TC_TLV_INVALID;
    first = !(data[i] & 128);
  }
  if (!first)
    return TC_TLV_INVALID;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_boolean(TC_bytes encoded, int* out)
{
  TC_bytes v;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = value(encoded, 1, &v);
  if (result != TC_TLV_OK)
    return result;
  if (v.length != 1 || (v.data[0] != 0 && v.data[0] != 255))
    return TC_TLV_INVALID;
  *out = v.data[0] != 0;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_null(TC_bytes encoded)
{
  TC_bytes v;
  TC_TLV_result result = value(encoded, 5, &v);
  return result != TC_TLV_OK ? result : v.length ? TC_TLV_INVALID : TC_TLV_OK;
}
TC_TLV_result TC_DER_sequence(TC_bytes encoded, TC_bytes* out)
{
  return OUTPUT_SEPARATE(encoded, out) ? value(encoded, 0x30, out) : TC_TLV_ARGUMENT;
}
TC_TLV_result TC_DER_set(TC_bytes encoded, TC_bytes* out)
{
  return OUTPUT_SEPARATE(encoded, out) ? value(encoded, 0x31, out) : TC_TLV_ARGUMENT;
}

static TC_TLV_result sequence_fields(TC_bytes encoded, TC_bytes* fields, size_t minimum,
                                     size_t maximum)
{
  const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, maximum + 1, 1};
  TC_bytes contents;
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_TLV_result result = TC_DER_sequence(encoded, &contents);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_reader_init(&reader, contents, TC_TLV_DER, &limits);
  if (result != TC_TLV_OK)
    return result;
  for (size_t i = 0; i < maximum; ++i) {
    result = TC_TLV_next(&reader, &element);
    if (result == TC_TLV_END && i >= minimum) {
      for (; i < maximum; ++i)
        fields[i] = (TC_bytes){NULL, 0};
      return TC_TLV_OK;
    }
    /* The enclosing SEQUENCE is complete, so MORE is a malformed child. */
    if (result != TC_TLV_OK)
      return TC_TLV_INVALID;
    fields[i] = element.encoded;
  }
  return TC_TLV_next(&reader, &element) == TC_TLV_END ? TC_TLV_OK : TC_TLV_INVALID;
}

static TC_TLV_result pair(TC_bytes encoded, TC_bytes fields[2], int optional_second)
{
  return sequence_fields(encoded, fields, optional_second ? 1 : 2, 2);
}

TC_TLV_result TC_DER_algorithm_identifier(TC_bytes encoded, TC_DER_algorithm* out)
{
  TC_bytes fields[2];
  TC_DER_algorithm algorithm;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = pair(encoded, fields, 1);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_oid(fields[0], &algorithm.oid);
  if (result != TC_TLV_OK)
    return result;
  algorithm.parameters = fields[1];
  *out = algorithm;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_positive_integer(TC_bytes encoded, TC_bytes* out)
{
  TC_bytes integer;
  int negative;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_DER_integer(encoded, &integer, &negative);
  if (result != TC_TLV_OK)
    return result;
  if (negative || (integer.length == 1 && integer.data[0] == 0))
    return TC_TLV_INVALID;
  if (integer.data[0] == 0) {
    ++integer.data;
    --integer.length;
  }
  *out = integer;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_rsa_public(TC_bytes encoded, TC_DER_rsa_public_key* out)
{
  TC_bytes fields[2];
  TC_DER_rsa_public_key key;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = pair(encoded, fields, 0);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_positive_integer(fields[0], &key.modulus);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_positive_integer(fields[1], &key.exponent);
  if (result != TC_TLV_OK)
    return result;
  *out = key;
  return TC_TLV_OK;
}

/* RFC 8017 appendix A.1.2 RSAPrivateKey. Version 1 (multi-prime, with an
 * optional otherPrimeInfos tenth field) is UNSUPPORTED. */
TC_TLV_result TC_DER_rsa_private(TC_bytes encoded, TC_DER_rsa_private_key* out)
{
  enum { TWO_PRIME = 0, MULTI_PRIME = 1 };
  TC_bytes fields[10];
  TC_DER_rsa_private_key key;
  uint32_t version;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = sequence_fields(encoded, fields, 9, 10);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_uint32(fields[0], &version);
  /* A version above UINT32_MAX is neither defined version. */
  if (result == TC_TLV_LIMIT)
    return TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  if (version == MULTI_PRIME)
    return TC_TLV_UNSUPPORTED;
  if (version != TWO_PRIME || fields[9].data)
    return TC_TLV_INVALID;
  TC_bytes* components[] = {&key.modulus,   &key.public_exponent, &key.private_exponent,
                            &key.prime1,    &key.prime2,          &key.exponent1,
                            &key.exponent2, &key.coefficient};
  for (size_t i = 0; i < sizeof components / sizeof *components; ++i) {
    result = TC_DER_positive_integer(fields[i + 1], components[i]);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_subject_public_key(TC_bytes encoded, TC_DER_public_key* out)
{
  TC_bytes fields[2];
  TC_DER_public_key key;
  TC_TLV_result result;
  unsigned unused;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = pair(encoded, fields, 0);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_algorithm_identifier(fields[0], &key.algorithm);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_bit_string(fields[1], &key.key, &unused);
  if (result != TC_TLV_OK)
    return result;
  if (unused || !key.key.length)
    return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_private_key_info(TC_bytes encoded, TC_DER_private_key* out)
{
  enum {
    VERSION = 0,
    ALGORITHM = 1,
    KEY = 2,
    OPTIONAL = 3,
    FIELD_COUNT = 5,
    OCTET_STRING = 0x04,
    IMPLICIT_ATTRIBUTES = 0xa0,
    IMPLICIT_PUBLIC_KEY = 0x81,
    PRIVATE_ONLY = 0,
    WITH_PUBLIC_KEY = 1
  };
  TC_bytes fields[FIELD_COUNT];
  TC_DER_private_key key;
  uint32_t version;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = sequence_fields(encoded, fields, 3, FIELD_COUNT);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_uint32(fields[VERSION], &version);
  /* RFC 5958 section 2: a later version, however large, is a newer syntax. */
  if (result == TC_TLV_LIMIT)
    return TC_TLV_UNSUPPORTED;
  if (result != TC_TLV_OK)
    return result;
  if (version > WITH_PUBLIC_KEY)
    return TC_TLV_UNSUPPORTED;
  result = TC_DER_algorithm_identifier(fields[ALGORITHM], &key.algorithm);
  if (result != TC_TLV_OK)
    return result;
  result = value(fields[KEY], OCTET_STRING, &key.key);
  if (result != TC_TLV_OK)
    return result;
  key.attributes = (TC_bytes){NULL, 0};
  key.public_key = (TC_bytes){NULL, 0};
  key.public_key_unused = 0;
  size_t next = OPTIONAL;
  if (fields[next].data && fields[next].data[0] == IMPLICIT_ATTRIBUTES) {
    result = value(fields[next], IMPLICIT_ATTRIBUTES, &key.attributes);
    if (result != TC_TLV_OK)
      return result;
    ++next;
  }
  if (fields[next].data) {
    TC_bytes bits;
    if (version != WITH_PUBLIC_KEY)
      return TC_TLV_INVALID;
    result = value(fields[next], IMPLICIT_PUBLIC_KEY, &bits);
    if (result != TC_TLV_OK)
      return result;
    result = tc_der_bit_string_contents(bits, &key.public_key, &key.public_key_unused);
    if (result != TC_TLV_OK)
      return result;
    ++next;
  } else if (version != PRIVATE_ONLY) {
    return TC_TLV_INVALID;
  }
  if (next < FIELD_COUNT && fields[next].data)
    return TC_TLV_INVALID;
  *out = key;
  return TC_TLV_OK;
}

TC_TLV_result TC_DER_ecdsa_signature(TC_bytes encoded, TC_DER_signature_pair* out)
{
  TC_bytes fields[2];
  TC_DER_signature_pair signature;
  TC_TLV_result result;
  if (!OUTPUT_SEPARATE(encoded, out))
    return TC_TLV_ARGUMENT;
  result = pair(encoded, fields, 0);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_positive_integer(fields[0], &signature.r);
  if (result != TC_TLV_OK)
    return result;
  result = TC_DER_positive_integer(fields[1], &signature.s);
  if (result != TC_TLV_OK)
    return result;
  *out = signature;
  return TC_TLV_OK;
}
#endif
