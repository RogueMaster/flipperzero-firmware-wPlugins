/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_sm_wire.h"
#include <tiny_crypto/tlv.h>
#include <string.h>

static int tag_is(const TC_TLV_element* element, uint8_t tag)
{
  return element->header.tag_length == 1 && element->header.tag[0] == tag;
}

static size_t length_octets(size_t length)
{
  size_t count = 1;
  if (length < 128) return 1;
  while (length) { ++count; length >>= 8; }
  return count;
}

static size_t encode_length(uint8_t* output, size_t length)
{
  const size_t count = length_octets(length);
  size_t i;
  if (count == 1) output[0] = (uint8_t)length;
  else {
    output[0] = (uint8_t)(0x80u | (count - 1));
    for (i = count - 1; i > 0; --i) {
      output[i] = (uint8_t)length;
      length >>= 8;
    }
  }
  return count;
}

TC_status example_piv_sm_begin(TC_PIV_SM* session, TC_PIV_SM_suite suite,
    const uint8_t host_identifier[8], TC_random_fn random, void* random_context,
    uint8_t* apdu, size_t capacity, size_t* written, TC_PIV_SM_workspace* workspace)
{
  TC_PIV_SM_handshake handshake;
  size_t request_length;
  if (!session || !host_identifier || !random || !apdu || !written || !workspace)
    return TC_ERROR;
  request_length = suite == TC_PIV_SM_CS2 ? 86 : suite == TC_PIV_SM_CS7 ? 118 : 0;
  if (!request_length || capacity < request_length) return TC_ERROR;
  if (TC_PIV_SM_begin(session,suite,host_identifier,random,random_context,
      &handshake,workspace) != TC_OK) return TC_ERROR;
  apdu[0] = 0; apdu[1] = 0x87; apdu[2] = (uint8_t)suite; apdu[3] = 4;
  apdu[4] = (uint8_t)(request_length - 6);
  apdu[5] = 0x7c; apdu[6] = (uint8_t)(request_length - 8);
  apdu[7] = 0x81; apdu[8] = (uint8_t)(handshake.public_key.length + 9);
  apdu[9] = 0;
  memcpy(apdu + 10,handshake.host_identifier.data,handshake.host_identifier.length);
  memcpy(apdu + 18,handshake.public_key.data,handshake.public_key.length);
  apdu[18 + handshake.public_key.length] = 0x82;
  apdu[19 + handshake.public_key.length] = 0;
  apdu[20 + handshake.public_key.length] = 0;
  *written = request_length;
  return TC_OK;
}

TC_status example_piv_sm_response_read(TC_PIV_SM_suite suite, TC_bytes encoded,
    ExamplePIVSMResponse* response)
{
  const size_t coordinate_bytes = suite == TC_PIV_SM_CS2 ? 32 :
    suite == TC_PIV_SM_CS7 ? 48 : 0;
  const size_t nonce_bytes = suite == TC_PIV_SM_CS2 ? 16 :
    suite == TC_PIV_SM_CS7 ? 24 : 0;
  const TC_TLV_limits limits = {encoded.length,encoded.length,4,2};
  TC_TLV_element outer, value;
  ExamplePIVSMResponse parsed = {0};
  size_t prefix;
  if (!coordinate_bytes || !encoded.data || !response) return TC_ERROR;
  if (TC_TLV_read(encoded.data,encoded.length,TC_TLV_ISO7816,&limits,&outer) != TC_TLV_OK ||
      !tag_is(&outer,0x7c) || outer.encoded.length != encoded.length ||
      TC_TLV_read(outer.value.data,outer.value.length,TC_TLV_ISO7816,&limits,&value) != TC_TLV_OK ||
      !tag_is(&value,0x82) || value.encoded.length != outer.value.length) return TC_ERROR;
  prefix = 1 + nonce_bytes + 16;
  if (value.value.length <= prefix || value.value.data[0] != 0) return TC_ERROR;
  parsed.peer.nonce = (TC_bytes){value.value.data + 1,nonce_bytes};
  parsed.peer.cryptogram = (TC_bytes){value.value.data + 1 + nonce_bytes,16};
  parsed.peer.certificate = (TC_bytes){value.value.data + prefix,value.value.length - prefix};
  if (TC_PIV_CVC_read(parsed.peer.certificate.data,parsed.peer.certificate.length,
      &parsed.cvc) != TC_TLV_OK || parsed.cvc.key_bits != coordinate_bytes * 8 ||
      parsed.cvc.role != TC_PIV_CVC_CARD_APPLICATION) return TC_ERROR;
  *response = parsed;
  return TC_OK;
}

TC_status example_piv_sm_finish(TC_PIV_SM* session, TC_bytes encoded,
    uint16_t transport_status, TC_bytes authenticated_key,
    TC_PIV_SM_workspace* workspace)
{
  ExamplePIVSMResponse response;
  if (!session || !workspace || session->state != TC_PIV_SM_ESTABLISHING)
    return TC_ERROR;
  if (transport_status != 0x9000 ||
      example_piv_sm_response_read((TC_PIV_SM_suite)session->suite,encoded,&response) != TC_OK) {
    TC_PIV_SM_clear(session);
    TC_secure_zero(workspace,sizeof *workspace);
    return TC_ERROR;
  }
  if (authenticated_key.length != response.cvc.public_key.length ||
      TC_ct_equal(authenticated_key.data,response.cvc.public_key.data,
        authenticated_key.length) != TC_OK) {
    TC_PIV_SM_clear(session);
    TC_secure_zero(workspace,sizeof *workspace);
    return TC_MISMATCH;
  }
  return TC_PIV_SM_finish(session,&response.peer,authenticated_key,workspace);
}

static int command_size(size_t length, unsigned has_le, size_t* padded, size_t* needed)
{
  size_t value, header, padding = 16 - length % 16;
  *padded = *needed = 0;
  if (length) {
    if (length > SIZE_MAX - padding) return 0;
    *padded = length + padding;
    if (*padded == SIZE_MAX) return 0;
    value = *padded + 1;
    header = 1 + length_octets(value);
    if (value > SIZE_MAX - header) return 0;
    *needed = value + header;
  }
  if (*needed > SIZE_MAX - 10 - 3 * has_le) return 0;
  *needed += 10 + 3 * has_le;
  return 1;
}

TC_status example_piv_sm_protect(TC_PIV_SM* session,
    const ExamplePIVSMCommand* command, uint8_t* output, size_t capacity,
    size_t* written, TC_PIV_SM_workspace* workspace)
{
  uint8_t header[16] = {0x0c,0,0,0,0x80};
  uint8_t object_header[6], le[] = {0x97,1,0}, tag[8];
  TC_bytes authenticated[4];
  TC_PIV_SM_protect_request request;
  size_t padded, needed, position = 0, actual = 0, count = 0, object_header_length = 0;
  uint8_t* ciphertext = NULL;
  if (!command || !session || !output || !written || !workspace || command->has_le > 1 ||
      (command->ins != 0xcb && command->ins != 0x20 && command->ins != 0x24 &&
       command->ins != 0x87) || (!command->data.length && command->ins != 0x20) ||
      !command_size(command->data.length,command->has_le,&padded,&needed) ||
      capacity < needed) return TC_ERROR;
  if (padded) {
    object_header[object_header_length++] = 0x87;
    object_header_length += encode_length(object_header + object_header_length,padded + 1);
    object_header[object_header_length++] = 1;
    position = object_header_length;
    ciphertext = output + position;
    position += padded;
  }
  if (command->has_le) position += sizeof le;
  header[1] = command->ins; header[2] = command->p1; header[3] = command->p2;
  authenticated[count++] = (TC_bytes){header,sizeof header};
  if (object_header_length) authenticated[count++] =
    (TC_bytes){object_header,object_header_length};
  if (padded) authenticated[count++] = (TC_bytes){ciphertext,padded};
  if (command->has_le) authenticated[count++] = (TC_bytes){le,sizeof le};
  request.plaintext = command->data;
  request.ciphertext = ciphertext;
  request.ciphertext_capacity = padded;
  request.authenticated = authenticated;
  request.authenticated_count = count;
  if (TC_PIV_SM_protect(session,&request,&actual,tag,workspace) != TC_OK) return TC_ERROR;
  if (actual != padded || position + 10 != needed) {
    TC_PIV_SM_clear(session);
    TC_secure_zero(output,needed);
    return TC_ERROR;
  }
  if (object_header_length) memcpy(output,object_header,object_header_length);
  position = object_header_length + padded;
  if (command->has_le) { memcpy(output + position,le,sizeof le); position += sizeof le; }
  output[position++] = 0x8e; output[position++] = 8;
  memcpy(output + position,tag,sizeof tag);
  position += sizeof tag;
  *written = needed;
  return TC_OK;
}

static int response_fields(TC_bytes encoded, TC_bytes* ciphertext,
    TC_bytes* authenticated, TC_bytes* tag, uint16_t* status)
{
  const TC_TLV_limits limits = {encoded.length,encoded.length,3,0};
  TC_TLV_reader reader;
  TC_TLV_element element;
  ciphertext->data = NULL; ciphertext->length = 0;
  if (TC_TLV_reader_init(&reader,encoded.data,encoded.length,TC_TLV_ISO7816,&limits) != TC_TLV_OK ||
      TC_TLV_next(&reader,&element) != TC_TLV_OK) return 0;
  if (tag_is(&element,0x87)) {
    if (element.value.length < 17 || element.value.data[0] != 1 ||
        (element.value.length - 1) % 16) return 0;
    *ciphertext = (TC_bytes){element.value.data + 1,element.value.length - 1};
    if (TC_TLV_next(&reader,&element) != TC_TLV_OK) return 0;
  }
  if (!tag_is(&element,0x99) || element.value.length != 2) return 0;
  *status = (uint16_t)((uint16_t)element.value.data[0] << 8 | element.value.data[1]);
  *authenticated = (TC_bytes){encoded.data,reader.offset};
  if (TC_TLV_next(&reader,&element) != TC_TLV_OK || !tag_is(&element,0x8e) ||
      element.value.length != 8 || reader.offset != encoded.length) return 0;
  *tag = element.value;
  return 1;
}

TC_status example_piv_sm_unprotect(TC_PIV_SM* session, TC_bytes encoded,
    uint16_t transport_status, uint8_t* output, size_t capacity,
    ExamplePIVSMResult* result, TC_PIV_SM_workspace* workspace)
{
  TC_bytes ciphertext, authenticated, tag;
  TC_PIV_SM_unprotect_request request;
  uint16_t status;
  size_t length;
  if (!session || !result || !workspace || (!output && capacity) ||
      transport_status != 0x9000 ||
      !response_fields(encoded,&ciphertext,&authenticated,&tag,&status)) {
    if (session && session->state == TC_PIV_SM_PENDING) TC_PIV_SM_clear(session);
    if (workspace) TC_secure_zero(workspace,sizeof *workspace);
    return TC_ERROR;
  }
  request.ciphertext = ciphertext;
  request.tag = tag;
  request.authenticated = &authenticated;
  request.authenticated_count = 1;
  {
    TC_status checked = TC_PIV_SM_unprotect(session,&request,output,capacity,&length,workspace);
    if (checked != TC_OK) return checked;
  }
  result->length = length;
  result->status = status;
  return TC_OK;
}
