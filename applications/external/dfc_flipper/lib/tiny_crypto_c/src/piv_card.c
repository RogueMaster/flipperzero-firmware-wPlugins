/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/twic_uuid.h>
#if TC_ENABLE_PIV_OBJECTS
#include "pki_reader_internal.h"
#include "pki_tree_internal.h"

enum {
  FASCN_BYTES = 25,
  UUID_BYTES = 16,
  UUID_URN_BYTES = 45,
  UUID_PREFIX_BYTES = 9,
  GENERAL_NAME_OTHER = 0,
  GENERAL_NAME_URI = 6
};

typedef enum {
  IDENTIFIERS_STRICT,
  IDENTIFIERS_TWIC_READER,
  IDENTIFIERS_PIV_AUTHENTICATION,
  IDENTIFIERS_TWIC_PIV_AUTHENTICATION
} identifiers_policy;

static unsigned ascii_lower(unsigned value) {
  return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static int uuid_prefix(TC_bytes text) {
  static const uint8_t prefix[] = "urn:uuid:";
  if (text.length < UUID_PREFIX_BYTES)
    return 0;
  for (size_t i = 0; i < UUID_PREFIX_BYTES; ++i)
    if (ascii_lower(text.data[i]) != prefix[i])
      return 0;
  return 1;
}

static int hex_digit(unsigned value) {
  value = ascii_lower(value);
  if (value >= '0' && value <= '9')
    return (int)(value - '0');
  if (value >= 'a' && value <= 'f')
    return (int)(value - 'a' + 10);
  return -1;
}

static TC_TLV_result uuid_read(TC_bytes text, uint8_t uuid[UUID_BYTES]) {
  if (!text.data || text.length != UUID_URN_BYTES || !uuid_prefix(text))
    return TC_TLV_INVALID;
  size_t position = UUID_PREFIX_BYTES;
  for (size_t i = 0; i < UUID_BYTES; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      if (text.data[position++] != '-')
        return TC_TLV_INVALID;
    const int high = hex_digit(text.data[position++]);
    const int low = hex_digit(text.data[position++]);
    if (high < 0 || low < 0)
      return TC_TLV_INVALID;
    uuid[i] = (uint8_t)(high * 16 + low);
  }
  return TC_TLV_OK;
}

static int uuid_profile(const uint8_t *uuid, TC_PIV_card_profile profile) {
  static const uint8_t nil[UUID_BYTES] = {0};
  if (profile == TC_TWIC_LEGACY_CARD)
    return !memcmp(uuid, nil, sizeof nil);
  if ((uuid[8] & 0xc0) != 0x80)
    return 0;
  if (profile == TC_TWIC_NEXGEN_CARD) {
    uint64_t number;
    return TC_TWIC_uuid_read((TC_bytes){uuid, UUID_BYTES}, &number) ==
           TC_TLV_OK;
  }
  const unsigned version = uuid[6] >> 4;
  return version == 1 || version == 4 || version == 5;
}

static TC_TLV_result read_fascn(const TC_X509_general_name *name,
                                TC_PIV_oid_profile profile,
                                const TC_TLV_limits *limits,
                                const tc_pki_tree_workspace *tree,
                                TC_PIV_card_identifiers *identifiers) {
  tc_pki_oid_value other;
  TC_TLV_reader value;
  TC_TLV_element octets;
  TC_TLV_result result = tc_pki_tree_oid_value(name->encoded, 0xa0, TC_TLV_DER,
                                               limits, tree, &other);
  if (result != TC_TLV_OK)
    return result;
  /* Recognize both namespaces before applying the selected acceptance policy.
   */
  if (tc_x509_path_charge(tree->work, other.oid.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (TC_PIV_oid_identify(other.oid, TC_PIV_OIDS_TWIC_COMPATIBLE) !=
      TC_PIV_OID_FASCN)
    return TC_TLV_OK;
  if (tc_x509_path_charge(tree->work, other.oid.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  if (identifiers->fascn.data ||
      TC_PIV_oid_identify(other.oid, profile) != TC_PIV_OID_FASCN)
    return TC_TLV_INVALID;
  result =
      tc_pki_tree_open(other.value, 0xa0, TC_TLV_DER, limits, tree, &value);
  if (result != TC_TLV_OK)
    return result;
  result = tc_pki_tree_field(&value, 4, tree, &octets);
  if (result != TC_TLV_OK)
    return result;
  if (!tc_pki_end(&value) || octets.value.length != FASCN_BYTES)
    return TC_TLV_INVALID;
  identifiers->fascn = octets.value;
  identifiers->fascn_oid = other.oid;
  return TC_TLV_OK;
}

static TC_TLV_result
identifiers_read(TC_bytes encoded, TC_PIV_card_profile profile,
                 identifiers_policy policy, const uint8_t card_guid[UUID_BYTES],
                 const TC_TLV_limits *limits, TC_TLV_frame *frames,
                 size_t frame_capacity, size_t *work,
                 TC_PIV_card_identifiers *out) {
  if (profile != TC_PIV_CARD && profile != TC_TWIC_LEGACY_CARD &&
      profile != TC_TWIC_NEXGEN_CARD)
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = tc_pki_reader_storage(
      encoded, limits, frames, frame_capacity, work, out, sizeof *out);
  if (result != TC_TLV_OK)
    return result;
  TC_PIV_card_identifiers identifiers = {0};
  TC_bytes uuid_urns[2] = {0};
  uint8_t uuids[2][UUID_BYTES] = {{0}};
  size_t uuid_count = 0;
  const tc_pki_tree_workspace tree = {frames, frame_capacity, work};
  const int authentication = policy == IDENTIFIERS_PIV_AUTHENTICATION ||
                             policy == IDENTIFIERS_TWIC_PIV_AUTHENTICATION;
  const int reader_policy = policy == IDENTIFIERS_TWIC_READER ||
                            policy == IDENTIFIERS_TWIC_PIV_AUTHENTICATION;
  const TC_PIV_oid_profile oids =
      profile == TC_PIV_CARD && policy != IDENTIFIERS_TWIC_PIV_AUTHENTICATION
          ? TC_PIV_OIDS_ONLY
          : TC_PIV_OIDS_TWIC_COMPATIBLE;
  TC_TLV_reader reader;
  /* One full framing scan, then bounded schema scans through every name. */
  result = tc_pki_tree_open(encoded, 0x30, TC_TLV_DER, limits, &tree, &reader);
  if (result != TC_TLV_OK)
    return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  if (tc_pki_end(&reader))
    return TC_TLV_INVALID;
  while (!tc_pki_end(&reader)) {
    TC_X509_general_name name;
    TC_TLV_reader next = reader;
    TC_TLV_element element;
    result = TC_TLV_next(&next, &element);
    if (result != TC_TLV_OK)
      return result;
    /* next_tree and GeneralName schema checks each scan the name's bytes. */
    for (unsigned scan = 0; scan < 2; ++scan)
      if (tc_x509_path_charge(work, element.encoded.length) != TC_TLV_OK)
        return TC_TLV_LIMIT;
    result = TC_X509_general_name_next(&reader, frames, frame_capacity, &name);
    if (result != TC_TLV_OK)
      return result;
    if (name.type == GENERAL_NAME_OTHER) {
      result = read_fascn(&name, oids, limits, &tree, &identifiers);
      if (result != TC_TLV_OK)
        return result;
    } else if (name.type == GENERAL_NAME_URI) {
      const size_t prefix_bytes = name.value.length < UUID_PREFIX_BYTES
                                      ? name.value.length
                                      : UUID_PREFIX_BYTES;
      if (tc_x509_path_charge(work, prefix_bytes) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (!uuid_prefix(name.value))
        continue;
      const size_t uuid_capacity = authentication ? 2 : 1;
      if (uuid_count == uuid_capacity)
        return TC_TLV_INVALID;
      if (tc_x509_path_charge(work, name.value.length) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      result = uuid_read(name.value, uuids[uuid_count]);
      if (result != TC_TLV_OK || !uuid_profile(uuids[uuid_count], profile))
        return TC_TLV_INVALID;
      uuid_urns[uuid_count++] = name.value;
    }
  }
  if (!identifiers.fascn.data ||
      (!reader_policy && profile != TC_TWIC_LEGACY_CARD && uuid_count == 0))
    return TC_TLV_INVALID;
  if (authentication && uuid_count) {
    size_t card_index = 0;
    unsigned matches = 0;
    for (size_t i = 0; i < uuid_count; ++i)
      if (!memcmp(uuids[i], card_guid, UUID_BYTES)) {
        card_index = i;
        ++matches;
      }
    if (matches != 1)
      return TC_TLV_INVALID;
    identifiers.uuid_urn = uuid_urns[card_index];
    if (uuid_count == 2) {
      const size_t cardholder_index = card_index ^ 1u;
      if ((uuids[cardholder_index][6] >> 4) != 4)
        return TC_TLV_INVALID;
      identifiers.cardholder_uuid_urn = uuid_urns[cardholder_index];
    }
  } else if (uuid_count) {
    identifiers.uuid_urn = uuid_urns[0];
  }
  if (tc_x509_path_charge(work, FASCN_BYTES) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  TC_FASCN decoded;
  result = TC_FASCN_read(identifiers.fascn, &decoded);
  if (result != TC_TLV_OK)
    return result;
  if (profile == TC_TWIC_NEXGEN_CARD && identifiers.uuid_urn.data) {
    if (tc_x509_path_charge(work, UUID_BYTES) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    int matched;
    result = TC_TWIC_uuid_match((TC_bytes){uuids[0], sizeof uuids[0]}, &decoded,
                                &matched);
    if (result != TC_TLV_OK)
      return result;
    if (!matched)
      return TC_TLV_INVALID;
  }
  *out = identifiers;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_card_identifiers_read(TC_bytes encoded,
                                           TC_PIV_card_profile profile,
                                           const TC_TLV_limits *limits,
                                           TC_TLV_frame *frames,
                                           size_t frame_capacity, size_t *work,
                                           TC_PIV_card_identifiers *out) {
  return identifiers_read(encoded, profile, IDENTIFIERS_STRICT, NULL, limits,
                          frames, frame_capacity, work, out);
}

TC_TLV_result TC_TWIC_card_identifiers_read(TC_bytes encoded,
                                            TC_PIV_card_profile profile,
                                            const TC_TLV_limits *limits,
                                            TC_TLV_frame *frames,
                                            size_t frame_capacity, size_t *work,
                                            TC_PIV_card_identifiers *out) {
  if (profile != TC_TWIC_LEGACY_CARD && profile != TC_TWIC_NEXGEN_CARD)
    return TC_TLV_ARGUMENT;
  return identifiers_read(encoded, profile, IDENTIFIERS_TWIC_READER, NULL,
                          limits, frames, frame_capacity, work, out);
}

TC_TLV_result TC_PIV_authentication_identifiers_read(
    TC_bytes encoded, TC_bytes card_guid, const TC_TLV_limits *limits,
    TC_TLV_frame *frames, size_t frame_capacity, size_t *work,
    TC_PIV_card_identifiers *out) {
  if (!card_guid.data || card_guid.length != UUID_BYTES)
    return TC_TLV_ARGUMENT;
  uint8_t expected[UUID_BYTES];
  memcpy(expected, card_guid.data, sizeof expected);
  return identifiers_read(encoded, TC_PIV_CARD, IDENTIFIERS_PIV_AUTHENTICATION,
                          expected, limits, frames, frame_capacity, work, out);
}

TC_TLV_result TC_TWIC_authentication_identifiers_read(
    TC_bytes encoded, TC_bytes card_guid, const TC_TLV_limits *limits,
    TC_TLV_frame *frames, size_t frame_capacity, size_t *work,
    TC_PIV_card_identifiers *out) {
  if (!card_guid.data || card_guid.length != UUID_BYTES)
    return TC_TLV_ARGUMENT;
  uint8_t expected[UUID_BYTES];
  memcpy(expected, card_guid.data, sizeof expected);
  return identifiers_read(encoded, TC_PIV_CARD,
                          IDENTIFIERS_TWIC_PIV_AUTHENTICATION, expected, limits,
                          frames, frame_capacity, work, out);
}

static TC_TLV_result
identifiers_match(const TC_PIV_card_identifiers *identifiers, TC_bytes fascn,
                  TC_bytes guid, int reader_policy, size_t *work,
                  int *matched) {
  enum {
    WRITE_COUNT = 2,
    READ_COUNT = 7,
    STORAGE_WORK = WRITE_COUNT * READ_COUNT + 1
  };
  TC_bytes writes[WRITE_COUNT], reads[READ_COUNT];
  size_t checks = STORAGE_WORK;
  if (!identifiers || !work || !matched || !fascn.data ||
      fascn.length != FASCN_BYTES || !guid.data || guid.length != UUID_BYTES ||
      tc_pki_storage_span(work, 1, sizeof *work, &writes[0]) != TC_TLV_OK ||
      tc_pki_storage_span(matched, 1, sizeof *matched, &writes[1]) !=
          TC_TLV_OK ||
      tc_pki_storage_span(identifiers, 1, sizeof *identifiers, &reads[0]) !=
          TC_TLV_OK)
    return TC_TLV_ARGUMENT;
  reads[1] = identifiers->fascn;
  reads[2] = identifiers->fascn_oid;
  reads[3] = identifiers->uuid_urn;
  reads[4] = identifiers->cardholder_uuid_urn;
  reads[5] = fascn;
  reads[6] = guid;
  TC_TLV_result result = tc_pki_storage_input(writes, 1, writes[1], &checks);
  if (result != TC_TLV_OK)
    return result;
  for (size_t i = 0; i < READ_COUNT; ++i) {
    result = tc_pki_storage_input(writes, WRITE_COUNT, reads[i], &checks);
    if (result != TC_TLV_OK)
      return result;
  }
  if (!identifiers->fascn.data || identifiers->fascn.length != FASCN_BYTES)
    return TC_TLV_INVALID;
  result = tc_x509_path_charge(work, STORAGE_WORK + FASCN_BYTES + UUID_BYTES);
  if (result != TC_TLV_OK)
    return result;
  uint8_t uuid[UUID_BYTES] = {0};
  if (identifiers->uuid_urn.data || identifiers->uuid_urn.length) {
    result = tc_x509_path_charge(work, identifiers->uuid_urn.length);
    if (result != TC_TLV_OK)
      return result;
    result = uuid_read(identifiers->uuid_urn, uuid);
    if (result != TC_TLV_OK)
      return result;
  }
  const int fascn_matches =
      !memcmp(identifiers->fascn.data, fascn.data, FASCN_BYTES);
  const int uuid_matches = (reader_policy && !identifiers->uuid_urn.length) ||
                           !memcmp(uuid, guid.data, UUID_BYTES);
  *matched = fascn_matches && uuid_matches;
  return TC_TLV_OK;
}

TC_TLV_result
TC_PIV_card_identifiers_match(const TC_PIV_card_identifiers *identifiers,
                              TC_bytes fascn, TC_bytes guid, size_t *work,
                              int *matched) {
  return identifiers_match(identifiers, fascn, guid, 0, work, matched);
}

TC_TLV_result
TC_TWIC_card_identifiers_match(const TC_PIV_card_identifiers *identifiers,
                               TC_bytes fascn, TC_bytes guid, size_t *work,
                               int *matched) {
  return identifiers_match(identifiers, fascn, guid, 1, work, matched);
}
#endif
