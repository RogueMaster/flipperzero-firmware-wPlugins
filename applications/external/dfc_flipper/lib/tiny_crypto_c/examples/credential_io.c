/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_io.h"
#include <string.h>

enum {
  CARD_SELECT = 0xa4, CARD_GET_DATA = 0xcb, CARD_GET_RESPONSE = 0xc0,
  CARD_TAG_LIST = 0x5c, CARD_SUCCESS = 0x9000,
  CARD_MORE = 0x61, CARD_WRONG_LENGTH = 0x6c,
  CARD_INITIAL_LENGTH = 255, CARD_HEADER_BYTES = 4
};

static const uint8_t application_aids[][9] = {
  {0xa0,0x00,0x00,0x03,0x08,0x00,0x00,0x10,0x00},
  {0xa0,0x00,0x00,0x03,0x67,0x20,0x00,0x00,0x01}
};

enum { CARD_TEMPLATE = 0x61, CARD_AID = 0x4f, CARD_VERSION_BYTES = 2 };
static const TC_TLV_limits identity_limits = {4096,4096,64,4};

static TC_TLV_result unique_field(TC_bytes input, uint8_t tag, TC_bytes* out)
{
  TC_TLV_reader reader;
  TC_TLV_element element;
  TC_bytes found = {NULL,0};
  TC_TLV_result result = TC_TLV_reader_init(&reader,input.data,input.length,
      TC_TLV_ISO7816,&identity_limits);
  if (result != TC_TLV_OK) return result;
  while ((result = TC_TLV_next(&reader,&element)) == TC_TLV_OK) {
    if (element.header.tag_length == 1 && element.header.tag[0] == tag) {
      if (found.data) return TC_TLV_INVALID;
      found = element.value;
    }
  }
  if (result != TC_TLV_END) return result;
  if (!found.data) return TC_TLV_INVALID;
  *out = found;
  return TC_TLV_OK;
}

TC_TLV_result example_card_identity(TC_bytes response,
    ExampleCardApplication expected, ExampleCardModel* out)
{
  TC_TLV_frame frames[4];
  TC_bytes properties = {0}, aid = {0};
  if (!out || (expected != EXAMPLE_CARD_PIV && expected != EXAMPLE_CARD_TWIC))
    return TC_TLV_ARGUMENT;
  TC_TLV_result result = TC_TLV_walk(response.data,response.length,TC_TLV_ISO7816,
      &identity_limits,frames,sizeof frames / sizeof *frames,NULL,NULL);
  if (result == TC_TLV_OK) result = unique_field(response,CARD_TEMPLATE,&properties);
  if (result == TC_TLV_OK) result = unique_field(properties,CARD_AID,&aid);
  if (result != TC_TLV_OK) return result == TC_TLV_MORE ? TC_TLV_INVALID : result;
  const size_t prefix_length = sizeof application_aids[0];
  if (aid.length != prefix_length + CARD_VERSION_BYTES ||
      memcmp(aid.data,application_aids[expected],prefix_length)) return TC_TLV_INVALID;
  /* The TWIC test bit and unfamiliar releases require an explicit workflow. */
  if (aid.data[prefix_length] != 1) return TC_TLV_UNSUPPORTED;
  const uint8_t minor = aid.data[prefix_length + 1];
  ExampleCardModel model;
  if (expected == EXAMPLE_CARD_PIV && minor == 0) model = EXAMPLE_CARD_MODEL_PIV;
  else if (expected == EXAMPLE_CARD_TWIC && minor == 1) model = EXAMPLE_CARD_MODEL_TWIC_LEGACY;
  else if (expected == EXAMPLE_CARD_TWIC && minor == 3) model = EXAMPLE_CARD_MODEL_TWIC_NEXGEN;
  else return TC_TLV_UNSUPPORTED;
  *out = model;
  return TC_TLV_OK;
}

ExampleCardResult example_twic_inventory_read(ExampleCardIO* io, ExampleCardModel model, ExampleCardReadMode mode,
    uint8_t* pool, size_t capacity, size_t max_object, size_t* work, ExampleTWICInventory* out)
{
  enum { LEGACY_OBJECTS = 3, REQUIRED_NEXGEN = 5, OBJECT_NOT_FOUND = 0x6a82, RESPONSE_TAG = 0x53 };
  /* TWIC Part 2 v5, sections 4.5 and 4.6.5. Optional NEXGEN objects follow required ones. */
  static const struct { uint16_t container; uint8_t tag[3]; } objects[] = {
    {EXAMPLE_TWIC_CHUID,{0x5f,0xc1,2}}, {EXAMPLE_TWIC_UNSIGNED_CHUID,{0x5f,0xc1,4}},
    {EXAMPLE_TWIC_FINGERPRINTS,{0xdf,0xc1,3}}, {EXAMPLE_TWIC_FACE,{0xdf,0xc1,8}},
    {EXAMPLE_TWIC_PRINTED,{0xdf,0xc1,9}}, {EXAMPLE_TWIC_IRIS,{0xdf,0xc1,0x21}},
    {EXAMPLE_TWIC_PERSONAL,{0xdf,0xc0,1}}, {EXAMPLE_TWIC_HANDWRITTEN,{0xdf,0xc0,2}}
  };
  static const uint8_t security_tag[] = {0xdf,0xc1,0x0f};
  if (!io || !io->transmit || !pool || !capacity || !work || !out || !max_object ||
      (mode != EXAMPLE_CARD_READ_SHORT && mode != EXAMPLE_CARD_READ_EXTENDED) ||
      max_object > SIZE_MAX - EXAMPLE_CARD_STATUS_BYTES ||
      (model != EXAMPLE_CARD_MODEL_TWIC_LEGACY && model != EXAMPLE_CARD_MODEL_TWIC_NEXGEN))
    return EXAMPLE_CARD_ARGUMENT;
  ExampleTWICInventory parsed = {0};
  ExampleCardResponse response;
  ExampleCardModel selected;
  size_t used = 0;
  const size_t transfer_capacity = max_object + EXAMPLE_CARD_STATUS_BYTES;
  const size_t first_capacity = capacity < transfer_capacity ? capacity : transfer_capacity;
  ExampleCardResult result = example_card_select(io,EXAMPLE_CARD_TWIC,pool,first_capacity,&response);
  if (result != EXAMPLE_CARD_OK) goto failure;
  if (response.length > *work) { result = EXAMPLE_CARD_LIMIT; goto failure; }
  *work -= response.length;
  if (example_card_identity((TC_bytes){pool,response.length},EXAMPLE_CARD_TWIC,&selected) != TC_TLV_OK ||
      selected != model) { result = EXAMPLE_CARD_PROTOCOL; goto failure; }
  const size_t count = model == EXAMPLE_CARD_MODEL_TWIC_LEGACY ? LEGACY_OBJECTS : EXAMPLE_TWIC_OBJECTS;
  const TC_TLV_limits limits = {max_object,max_object,1,1};
  for (size_t i = 0; i <= count; ++i) {
    const uint8_t* tag = i == count ? security_tag : objects[i].tag;
    const size_t remaining = capacity - used;
    const size_t available = remaining < transfer_capacity ? remaining : transfer_capacity;
    if (!available) { result = EXAMPLE_CARD_LIMIT; goto failure; }
    result = example_card_object_read(io,mode,tag,3,pool + used,available,&response);
    if (result == EXAMPLE_CARD_STATUS && i >= REQUIRED_NEXGEN && i < count &&
        response.status == OBJECT_NOT_FOUND && !response.length) continue;
    if (result != EXAMPLE_CARD_OK) goto failure;
    if (response.length > *work) { result = EXAMPLE_CARD_LIMIT; goto failure; }
    *work -= response.length;
    TC_TLV_element field;
    const TC_TLV_result framing = TC_TLV_read(pool + used,response.length,TC_TLV_ISO7816,&limits,&field);
    if (framing != TC_TLV_OK) {
      result = framing == TC_TLV_LIMIT ? EXAMPLE_CARD_LIMIT : EXAMPLE_CARD_PROTOCOL;
      goto failure;
    }
    if (field.header.tag_length != 1 || field.header.tag[0] != RESPONSE_TAG ||
        field.encoded.length != response.length || !field.value.length) {
      result = EXAMPLE_CARD_PROTOCOL; goto failure;
    }
    if (i == count) parsed.security = field.encoded;
    else parsed.objects[parsed.count++] = (ExampleTWICObject){objects[i].container,field.value};
    used += response.length;
  }
  *out = parsed;
  return EXAMPLE_CARD_OK;
failure:
  TC_secure_zero(pool,capacity);
  io->stopped = 1;
  return result;
}

static ExampleCardResult exchange(ExampleCardIO* io, const uint8_t* command,
    size_t command_length, uint8_t* response, size_t capacity, size_t* received)
{
  ExampleCardResult result;
  if (io->stopped) return EXAMPLE_CARD_TRANSPORT;
  if (!io->exchanges_left) result = EXAMPLE_CARD_LIMIT;
  else {
    --io->exchanges_left;
    if (!io->transmit(io->context,command,command_length,response,capacity,received))
      result = EXAMPLE_CARD_TRANSPORT;
    else if (*received < EXAMPLE_CARD_STATUS_BYTES || *received > capacity)
      result = EXAMPLE_CARD_PROTOCOL;
    else return EXAMPLE_CARD_OK;
  }
  io->stopped = 1;
  return result;
}

static ExampleCardResult read_response(ExampleCardIO* io, uint8_t* command,
    size_t command_length, size_t length_bytes, int allow_correction,
    uint8_t* buffer, size_t capacity, ExampleCardResponse* out)
{
  size_t used = 0;
  int corrected = 0;
  ExampleCardResult result = EXAMPLE_CARD_PROTOCOL;
  if (!io || !io->transmit || !buffer || !out) return EXAMPLE_CARD_ARGUMENT;
  if (io->stopped) return EXAMPLE_CARD_TRANSPORT;
  for (;;) {
    size_t expected = command[command_length - 1];
    if (length_bytes == 2)
      expected |= (size_t)command[command_length - 2] << 8;
    if (!expected) expected = EXAMPLE_CARD_RESPONSE_BYTES;
    if (!io->exchanges_left || capacity - used < expected + EXAMPLE_CARD_STATUS_BYTES) {
      result = EXAMPLE_CARD_LIMIT;
      break;
    }
    size_t received = 0;
    result = exchange(io,command,command_length,buffer + used,
        expected + EXAMPLE_CARD_STATUS_BYTES,&received);
    if (result != EXAMPLE_CARD_OK) break;
    result = EXAMPLE_CARD_PROTOCOL;
    const size_t payload = received - EXAMPLE_CARD_STATUS_BYTES;
    const uint8_t sw1 = buffer[used + payload], sw2 = buffer[used + payload + 1];
    /* Status bytes are overwritten by the next chunk or cleared on completion. */
    TC_secure_zero(buffer + used + payload,EXAMPLE_CARD_STATUS_BYTES);
    if (sw1 == CARD_WRONG_LENGTH) {
      if (!allow_correction || payload || corrected) break;
      command[command_length - 1] = sw2;
      corrected = 1;
      continue;
    }
    used += payload;
    if (sw1 == CARD_MORE) {
      command[0] = 0;
      command[1] = CARD_GET_RESPONSE;
      command[2] = command[3] = 0;
      command[CARD_HEADER_BYTES] = sw2;
      command_length = CARD_HEADER_BYTES + 1;
      length_bytes = 1;
      allow_correction = 1;
      corrected = 0;
      continue;
    }
    out->length = used;
    out->status = (uint16_t)((unsigned)sw1 << 8 | sw2);
    return out->status == CARD_SUCCESS ? EXAMPLE_CARD_OK : EXAMPLE_CARD_STATUS;
  }
  io->stopped = 1;
  TC_secure_zero(buffer,capacity);
  return result;
}

ExampleCardResult example_card_verify_pin(ExampleCardIO* io, ExampleCardPIN* guard,
    const uint8_t* digits, size_t length, uint16_t* status)
{
  enum { PIN_MIN = 6, PIN_BYTES = 8, PIN_VERIFY = 0x20, PIN_LOCAL = 0x80,
    PIN_PADDING = 0xff, PIN_RETRIES = 0x63c0, PIN_MAX_RETRIES = 10 };
  uint8_t command[CARD_HEADER_BYTES + 1 + PIN_BYTES] = {0,PIN_VERIFY,0,PIN_LOCAL};
  uint8_t response[EXAMPLE_CARD_STATUS_BYTES];
  size_t received = 0;
  if (!io || !io->transmit || !guard || !digits || !status ||
      length < PIN_MIN || length > PIN_BYTES) return EXAMPLE_CARD_ARGUMENT;
  for (size_t i = 0; i < length; ++i)
    if (digits[i] < '0' || digits[i] > '9') return EXAMPLE_CARD_ARGUMENT;
  if (guard->used) return EXAMPLE_CARD_REFUSED;
  /* Consume the allowance before querying, including ambiguous transport exits. */
  guard->used = 1;
  ExampleCardResult result = exchange(io,command,CARD_HEADER_BYTES,response,sizeof response,&received);
  if (result != EXAMPLE_CARD_OK) goto cleanup;
  *status = (uint16_t)((unsigned)response[0] << 8 | response[1]);
  if (*status == CARD_SUCCESS) goto cleanup;
  if (*status < PIN_RETRIES + 2 || *status > PIN_RETRIES + PIN_MAX_RETRIES) {
    result = EXAMPLE_CARD_REFUSED;
    goto cleanup;
  }
  command[CARD_HEADER_BYTES] = PIN_BYTES;
  memset(command + CARD_HEADER_BYTES + 1,PIN_PADDING,PIN_BYTES);
  memcpy(command + CARD_HEADER_BYTES + 1,digits,length);
  result = exchange(io,command,sizeof command,response,sizeof response,&received);
  if (result != EXAMPLE_CARD_OK) goto cleanup;
  *status = (uint16_t)((unsigned)response[0] << 8 | response[1]);
  if (*status != CARD_SUCCESS) result = EXAMPLE_CARD_STATUS;
cleanup:
  TC_secure_zero(command,sizeof command);
  TC_secure_zero(response,sizeof response);
  if (result != EXAMPLE_CARD_OK) io->stopped = 1;
  return result;
}

ExampleCardResult example_card_select(ExampleCardIO* io, ExampleCardApplication application,
    uint8_t* buffer, size_t capacity, ExampleCardResponse* out)
{
  uint8_t command[CARD_HEADER_BYTES + 1 + sizeof application_aids[0] + 1] = {0,CARD_SELECT,4,0};
  if (application != EXAMPLE_CARD_PIV && application != EXAMPLE_CARD_TWIC)
    return EXAMPLE_CARD_ARGUMENT;
  command[CARD_HEADER_BYTES] = sizeof application_aids[0];
  memcpy(command + CARD_HEADER_BYTES + 1,application_aids[application],sizeof application_aids[0]);
  command[sizeof command - 1] = application == EXAMPLE_CARD_PIV ? 0 : CARD_INITIAL_LENGTH;
  return read_response(io,command,sizeof command,1,1,buffer,capacity,out);
}

ExampleCardResult example_card_read(ExampleCardIO* io, const uint8_t* tag,
    size_t tag_length, uint8_t* buffer, size_t capacity, ExampleCardResponse* out)
{
  uint8_t command[CARD_HEADER_BYTES + 1 + 2 + 3 + 1] = {0,CARD_GET_DATA,0x3f,0xff};
  if (!tag || tag_length < 1 || tag_length > 3) return EXAMPLE_CARD_ARGUMENT;
  command[CARD_HEADER_BYTES] = (uint8_t)(2 + tag_length);
  command[CARD_HEADER_BYTES + 1] = CARD_TAG_LIST;
  command[CARD_HEADER_BYTES + 2] = (uint8_t)tag_length;
  memcpy(command + CARD_HEADER_BYTES + 3,tag,tag_length);
  const size_t length = CARD_HEADER_BYTES + 4 + tag_length;
  command[length - 1] = CARD_INITIAL_LENGTH;
  return read_response(io,command,length,1,1,buffer,capacity,out);
}

ExampleCardResult example_card_read_extended(ExampleCardIO* io, const uint8_t* tag,
    size_t tag_length, uint8_t* buffer, size_t capacity, ExampleCardResponse* out)
{
  enum { LC_BYTES = 3, LE_BYTES = 2, TAG_LIST_BYTES = 2, MAX_TAG_BYTES = 3 };
  uint8_t command[CARD_HEADER_BYTES + LC_BYTES + TAG_LIST_BYTES + MAX_TAG_BYTES + LE_BYTES] =
      {0,CARD_GET_DATA,0x3f,0xff,0,0,0};
  if (!tag || !tag_length || tag_length > MAX_TAG_BYTES ||
      capacity <= EXAMPLE_CARD_STATUS_BYTES) return EXAMPLE_CARD_ARGUMENT;
  const size_t data_offset = CARD_HEADER_BYTES + LC_BYTES;
  command[data_offset - 1] = (uint8_t)(TAG_LIST_BYTES + tag_length);
  command[data_offset] = CARD_TAG_LIST;
  command[data_offset + 1] = (uint8_t)tag_length;
  memcpy(command + data_offset + TAG_LIST_BYTES,tag,tag_length);
  const size_t le_offset = data_offset + TAG_LIST_BYTES + tag_length;
  size_t expected = capacity - EXAMPLE_CARD_STATUS_BYTES;
  if (expected > UINT16_MAX) expected = UINT16_MAX;
  command[le_offset] = (uint8_t)(expected >> 8);
  command[le_offset + 1] = (uint8_t)expected;
  return read_response(io,command,le_offset + LE_BYTES,LE_BYTES,0,buffer,capacity,out);
}

ExampleCardResult example_card_object_read(ExampleCardIO* io, ExampleCardReadMode mode,
    const uint8_t* tag, size_t tag_length, uint8_t* buffer, size_t capacity,
    ExampleCardResponse* out)
{
  enum { END_OF_OBJECT = 0x6282, CONTAINER_TAG = 0x53 };
  if (!out || (mode != EXAMPLE_CARD_READ_SHORT && mode != EXAMPLE_CARD_READ_EXTENDED))
    return EXAMPLE_CARD_ARGUMENT;
  ExampleCardResponse response;
  ExampleCardResult result = mode == EXAMPLE_CARD_READ_EXTENDED ?
      example_card_read_extended(io,tag,tag_length,buffer,capacity,&response) :
      example_card_read(io,tag,tag_length,buffer,capacity,&response);
  if (result != EXAMPLE_CARD_OK && result != EXAMPLE_CARD_STATUS) return result;
  if (result == EXAMPLE_CARD_STATUS && response.status != END_OF_OBJECT) {
    *out = response;
    return result;
  }
  const TC_TLV_limits limits = {capacity,capacity,1,1};
  TC_TLV_element container;
  if (TC_TLV_read(buffer,response.length,TC_TLV_ISO7816,&limits,&container) != TC_TLV_OK ||
      container.header.tag_length != 1 || container.header.tag[0] != CONTAINER_TAG ||
      container.encoded.length != response.length) {
    TC_secure_zero(buffer,capacity);
    io->stopped = 1;
    return EXAMPLE_CARD_PROTOCOL;
  }
  *out = response;
  return EXAMPLE_CARD_OK;
}

/* Lengths are bounded by one 3072-bit RSA representative and its headers. */
static size_t authentication_length(uint8_t* output, size_t length)
{
  if (length < 128) {
    output[0] = (uint8_t)length;
    return 1;
  }
  if (length <= 255) {
    output[0] = 0x81;
    output[1] = (uint8_t)length;
    return 2;
  }
  output[0] = 0x82;
  output[1] = (uint8_t)(length >> 8);
  output[2] = (uint8_t)length;
  return 3;
}

ExampleCardResult example_card_authenticate(ExampleCardIO* io, ExampleCardAlgorithm algorithm,
    ExampleCardKeyReference reference, TC_bytes challenge,
    uint8_t* buffer, size_t capacity, ExampleCardResponse* out)
{
  enum { AUTHENTICATE = 0x87, CHAINED = 0x10,
    AUTH_TEMPLATE = 0x7c, AUTH_RESPONSE = 0x82, AUTH_CHALLENGE = 0x81,
    MAX_CHALLENGE = 384, MAX_LENGTH_BYTES = 3, MAX_CHUNK = 255,
    MIN_RESPONSE_CAPACITY = 514 };
  size_t expected;
  switch (algorithm) {
    case EXAMPLE_CARD_ALGORITHM_RSA_3072: expected = 384; break;
    case EXAMPLE_CARD_ALGORITHM_RSA_1024: expected = 128; break;
    case EXAMPLE_CARD_ALGORITHM_RSA_2048: expected = 256; break;
    case EXAMPLE_CARD_ALGORITHM_EC_P256: expected = 32; break;
    case EXAMPLE_CARD_ALGORITHM_EC_P384: expected = 48; break;
    default: return EXAMPLE_CARD_ARGUMENT;
  }
  if (reference != EXAMPLE_CARD_KEY_CARD_AUTHENTICATION &&
      reference != EXAMPLE_CARD_KEY_PIV_AUTHENTICATION) return EXAMPLE_CARD_ARGUMENT;
  if (!io || !io->transmit || !challenge.data || challenge.length != expected ||
      !buffer || capacity < MIN_RESPONSE_CAPACITY || !out) return EXAMPLE_CARD_ARGUMENT;
  if (io->stopped) return EXAMPLE_CARD_TRANSPORT;

  uint8_t payload[MAX_CHALLENGE + 2 * (1 + MAX_LENGTH_BYTES) + 2];
  uint8_t length_bytes[MAX_LENGTH_BYTES];
  uint8_t command[CARD_HEADER_BYTES + 1 + MAX_CHUNK + 1];
  uint8_t status[EXAMPLE_CARD_STATUS_BYTES];
  const size_t challenge_length_bytes = authentication_length(length_bytes,challenge.length);
  size_t used = 0;
  payload[used++] = AUTH_TEMPLATE;
  used += authentication_length(payload + used,2 + 1 + challenge_length_bytes + challenge.length);
  payload[used++] = AUTH_RESPONSE;
  payload[used++] = 0;
  payload[used++] = AUTH_CHALLENGE;
  memcpy(payload + used,length_bytes,challenge_length_bytes);
  used += challenge_length_bytes;
  memcpy(payload + used,challenge.data,challenge.length);
  used += challenge.length;

  ExampleCardResult result = EXAMPLE_CARD_PROTOCOL;
  size_t offset = 0;
  while (offset < used) {
    const size_t remaining = used - offset;
    const int chained = remaining > MAX_CHUNK;
    const size_t chunk = chained ? MAX_CHUNK : remaining;
    command[0] = chained ? CHAINED : 0;
    command[1] = AUTHENTICATE;
    command[2] = algorithm;
    command[3] = (uint8_t)reference;
    command[CARD_HEADER_BYTES] = (uint8_t)chunk;
    memcpy(command + CARD_HEADER_BYTES + 1,payload + offset,chunk);
    size_t command_length = CARD_HEADER_BYTES + 1 + chunk;
    if (!chained) {
      command[command_length++] = 0;
      result = read_response(io,command,command_length,1,0,buffer,capacity,out);
      break;
    }
    size_t received = 0;
    result = exchange(io,command,command_length,status,sizeof status,&received);
    if (result != EXAMPLE_CARD_OK) break;
    if (status[0] != (CARD_SUCCESS >> 8) || status[1] != (CARD_SUCCESS & 0xff)) {
      result = EXAMPLE_CARD_STATUS;
      out->length = 0;
      out->status = (uint16_t)((unsigned)status[0] << 8 | status[1]);
      break;
    }
    offset += chunk;
  }
  TC_secure_zero(payload,sizeof payload);
  TC_secure_zero(command,sizeof command);
  TC_secure_zero(status,sizeof status);
  if (result != EXAMPLE_CARD_OK) {
    io->stopped = 1;
    TC_secure_zero(buffer,capacity);
  }
  return result;
}
