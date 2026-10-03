/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Card Capability Container reader. */
#include <tiny_crypto/piv_card_objects.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include "piv_card_objects_internal.h"
#include "pki_internal.h"

/* One Table 9 row: the tag and the permitted value lengths. */
typedef struct {
  uint8_t tag;
  uint8_t kind;
  uint8_t length;
} ccc_field;

enum {
  LENGTH_EMPTY_OR, /* empty or exactly length bytes */
  LENGTH_UP_TO,    /* 0 to length bytes */
  LENGTH_EXACT     /* exactly length bytes */
};

enum {
  CARD_IDENTIFIER = 0,
  CONTAINER_VERSION,
  GRAMMAR_VERSION,
  CARD_URL,
  PKCS15,
  DATA_MODEL,
  ACCESS_CONTROL_RULES,
  CCC_FIELDS = 13
};

/* SP 800-73-5 Part 1 Table 9, in order. */
static const ccc_field ccc_fields[CCC_FIELDS] = {
    {0xf0, LENGTH_EMPTY_OR, 21}, {0xf1, LENGTH_EMPTY_OR, 1}, {0xf2, LENGTH_EMPTY_OR, 1},
    {0xf3, LENGTH_UP_TO, 128},   {0xf4, LENGTH_EMPTY_OR, 1}, {0xf5, LENGTH_EXACT, 1},
    {0xf6, LENGTH_EMPTY_OR, 17}, {0xf7, LENGTH_EXACT, 0},    {0xfa, LENGTH_EXACT, 0},
    {0xfb, LENGTH_EXACT, 0},     {0xfc, LENGTH_EXACT, 0},    {0xfd, LENGTH_EXACT, 0},
    {0xfe, LENGTH_EXACT, 0}};

/* SP 800-73-4 Part 1 Table 8 places the optional Extended Application
 * CardURL (E3) and Security Object Buffer (B4) before FE. */
static const ccc_field legacy_fields[2] = {{0xe3, LENGTH_UP_TO, 48}, {0xb4, LENGTH_UP_TO, 48}};
enum { ERROR_DETECTION_FIELD = CCC_FIELDS - 1 };

static int length_allowed(const ccc_field* field, size_t length)
{
  if (field->kind == LENGTH_UP_TO)
    return length <= field->length;
  if (field->kind == LENGTH_EMPTY_OR && !length)
    return 1;
  return length == field->length;
}

static TC_TLV_result field_read(TC_TLV_reader* reader, const ccc_field* field, TC_bytes* value)
{
  TC_TLV_element element;
  TC_TLV_result result = tc_pki_field(reader, field->tag, &element);
  if (result != TC_TLV_OK)
    return result;
  if (!length_allowed(field, element.value.length))
    return TC_TLV_INVALID;
  *value = element.value;
  return TC_TLV_OK;
}

/* The next tag in reader, or 0 at the end. */
static unsigned next_tag(const TC_TLV_reader* reader)
{
  return reader->offset < reader->input.length ? reader->input.data[reader->offset] : 0;
}

/* A one-byte version field, or -1 when empty. */
static int byte_value(TC_bytes value)
{
  return value.length ? value.data[0] : -1;
}

static TC_TLV_result ccc_read(TC_bytes encoded, TC_PIV_container_encoding encoding, TC_PIV_CCC* out)
{
  TC_bytes contents, values[CCC_FIELDS], legacy;
  TC_TLV_reader reader;
  TC_TLV_result result = tc_piv_object_contents(encoded, encoding, out, sizeof *out, &contents);
  if (result != TC_TLV_OK)
    return result;
  result = TC_TLV_reader_init(&reader, contents, TC_TLV_ISO7816, &tc_piv_object_limits);
  for (size_t i = 0; result == TC_TLV_OK && i < ERROR_DETECTION_FIELD; ++i)
    result = field_read(&reader, &ccc_fields[i], &values[i]);
  for (size_t i = 0; result == TC_TLV_OK && i < 2; ++i)
    if (next_tag(&reader) == legacy_fields[i].tag)
      result = field_read(&reader, &legacy_fields[i], &legacy);
  if (result == TC_TLV_OK)
    result =
        field_read(&reader, &ccc_fields[ERROR_DETECTION_FIELD], &values[ERROR_DETECTION_FIELD]);
  if (result == TC_TLV_OK && !tc_pki_end(&reader))
    result = TC_TLV_INVALID;
  if (result != TC_TLV_OK)
    return result;
  out->card_identifier = values[CARD_IDENTIFIER];
  out->card_url = values[CARD_URL];
  out->access_control_rules = values[ACCESS_CONTROL_RULES];
  out->container_version = byte_value(values[CONTAINER_VERSION]);
  out->grammar_version = byte_value(values[GRAMMAR_VERSION]);
  out->pkcs15 = byte_value(values[PKCS15]);
  out->data_model = values[DATA_MODEL].data[0];
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_CCC_read(TC_bytes encoded, TC_PIV_container_encoding encoding, TC_PIV_CCC* out)
{
  return tc_piv_object_result(ccc_read(encoded, encoding, out));
}
#endif
