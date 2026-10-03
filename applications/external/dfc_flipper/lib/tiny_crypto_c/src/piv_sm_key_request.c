/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key establishment command of PIV secure messaging (SP 800-73-5 Part 2
 * 4.1.1 steps H1 to H3 and 4.1.8). */
#include <tiny_crypto/piv_sm_apdu.h>
#if TC_ENABLE_PIV_SM_APDU
#include "internal.h"
#include "piv_link_internal.h"
#include "piv_sm_apdu_internal.h"
#include "piv_template_internal.h"

enum {
  KEY_ESTABLISHMENT = 0x04, /* P2 of the key establishment GENERAL AUTHENTICATE */
  CONTROL_BYTES = 1,        /* CB_H and CB_ICC */
  ID_BYTES = 8,             /* ID_sH */
  CRYPTOGRAM_BYTES = 16,    /* AuthCryptogram_ICC */
  /* 7C L 81 L {CB_H, ID_sH, Q_eH} 82 00 with the largest built point. */
  REQUEST_MAX_BYTES = TC_PIV_SM_KEY_REQUEST_BYTES,
  POINT_BYTES = 1 + 2 * TC_PIV_SM_COORDINATE_BYTES
};
/* The advertised command scratch, TC_PIV_EXTENDED_SCRATCH_BYTES, holds the
 * request. */
typedef char tc_key_request_size_check[REQUEST_MAX_BYTES == 2 + 2 + CONTROL_BYTES + ID_BYTES +
                                                                POINT_BYTES + 2 &&
                                               TC_APDU_EXTENDED_COMMAND_BYTES(REQUEST_MAX_BYTES) <=
                                                   TC_PIV_EXTENDED_SCRATCH_BYTES
                                           ? 1
                                           : -1];

static const uint8_t host_control = 0x00; /* CB_H (H1) */
static const uint8_t response_tag = TC_PIV_TEMPLATE_RESPONSE;
static const uint8_t card_cvc_tag[] = {0x7f, 0x21};

static int suite_known(TC_PIV_SM_suite suite)
{
  return suite == TC_PIV_SM_CS2 || suite == TC_PIV_SM_CS7;
}

static int suite_built(TC_PIV_SM_suite suite)
{
  return suite == TC_PIV_SM_CS2 ? TC_PIV_SM_ENABLE_CS2 : TC_PIV_SM_ENABLE_CS7;
}

/* 7C {81 {CB_H 00 || ID_sH || Q_eH}, 82 00} (4.1.8). Returns the length,
 * at most REQUEST_MAX_BYTES for both suites. */
static size_t request_write(const TC_PIV_SM_handshake* handshake, uint8_t* out)
{
  const tc_piv_template_item items[] = {
      {TC_PIV_TEMPLATE_CHALLENGE,
       {{&host_control, CONTROL_BYTES}, handshake->host_identifier, handshake->public_key}},
      {TC_PIV_TEMPLATE_RESPONSE, {{NULL, 0}}}};
  return (size_t)(tc_piv_template_write(out, items, 2) - out);
}

/* 7C {82 {CB_ICC || N_ICC || AuthCryptogram_ICC || C_ICC}} with C_ICC one
 * complete 7F21 DO (Part 2 4.1.5 Table 19). */
static int peer_read(TC_bytes answer, TC_PIV_SM_suite suite, TC_PIV_SM_peer* out)
{
  const size_t nonce_bytes = suite == TC_PIV_SM_CS2 ? 16u : 24u;
  const size_t prefix = CONTROL_BYTES + nonce_bytes + CRYPTOGRAM_BYTES;
  TC_bytes value;
  if (tc_piv_template_read(answer, &response_tag, 1, &value) != TC_TLV_OK ||
      value.length <= prefix || value.length - prefix > TC_PIV_SM_CARD_CVC_MAX_BYTES)
    return 0;
  const TC_bytes certificate = {value.data + prefix, value.length - prefix};
  const TC_TLV_limits limits = {certificate.length, certificate.length, 1, 0};
  TC_TLV_element cvc;
  if (TC_TLV_read(certificate, TC_TLV_ISO7816, &limits, &cvc) != TC_TLV_OK ||
      cvc.header.tag_length != sizeof card_cvc_tag ||
      memcmp(cvc.header.tag, card_cvc_tag, sizeof card_cvc_tag) ||
      cvc.encoded.length != certificate.length)
    return 0;
  out->card_control = value.data[0];
  out->nonce = (TC_bytes){value.data + CONTROL_BYTES, nonce_bytes};
  out->cryptogram = (TC_bytes){value.data + CONTROL_BYTES + nonce_bytes, CRYPTOGRAM_BYTES};
  out->certificate = certificate;
  return 1;
}

/* 1 when length bytes at data are disjoint from *link, its command scratch
 * and a session bound earlier. The session argument may be that bound
 * session, since the request replaces it. */
static int link_storage_disjoint(const TC_PIV_link* link, const void* data, size_t length,
                                 int session)
{
  return tc_internal_ranges_disjoint(data, length, link, sizeof *link) &&
         tc_internal_ranges_disjoint(data, length, link->channel.scratch,
                                     link->channel.scratch_capacity) &&
         (!link->sm || (session && data == link->sm) ||
          tc_internal_ranges_disjoint(data, length, link->sm, sizeof(TC_PIV_SM)));
}

/* Argument checks at the public entry. */
static int request_valid(const TC_PIV_link* link, const TC_PIV_SM* session, TC_PIV_SM_suite suite,
                         const uint8_t host_id[8], TC_random_source random, TC_buffer response,
                         const TC_PIV_SM_peer* peer, const TC_PIV_SM_workspace* workspace)
{
  if (!tc_piv_link_ready(link) || !session || !host_id || !peer || !workspace || !random.fill ||
      !suite_known(suite) || !tc_piv_response_valid(link, response, peer, sizeof *peer))
    return 0;
  const TC_bytes storage[] = {{response.data, response.capacity},
                              {(const uint8_t*)session, sizeof *session},
                              {(const uint8_t*)workspace, sizeof *workspace},
                              {(const uint8_t*)peer, sizeof *peer},
                              {host_id, ID_BYTES}};
  const size_t count = sizeof storage / sizeof storage[0];
  for (size_t i = 0; i < count; ++i) {
    if (i && !link_storage_disjoint(link, storage[i].data, storage[i].length, i == 1))
      return 0;
    for (size_t j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(storage[i].data, storage[i].length, storage[j].data,
                                       storage[j].length))
        return 0;
  }
  return 1;
}

/* End a failed establishment: the session and workspace hold key material. */
static TC_PIV_result request_fail(TC_PIV_link* link, TC_PIV_SM* session,
                                  TC_PIV_SM_workspace* workspace, TC_buffer response,
                                  uint16_t status, TC_PIV_result result)
{
  TC_PIV_SM_clear(session);
  TC_secure_zero(workspace, sizeof *workspace);
  return tc_piv_link_fail(link, response, status, result);
}

TC_PIV_result TC_PIV_SM_key_request(TC_PIV_link* link, TC_PIV_SM* session, TC_PIV_SM_suite suite,
                                    const uint8_t host_id[8], TC_random_source random,
                                    TC_buffer response, TC_PIV_SM_peer* peer,
                                    TC_PIV_SM_workspace* workspace)
{
  if (!request_valid(link, session, suite, host_id, random, response, peer, workspace))
    return TC_PIV_ARGUMENT;
  if (!suite_built(suite) || (link->sm_suite && link->sm_suite != (uint8_t)suite))
    return TC_PIV_UNSUPPORTED;
  /* Re-keying a secured link needs TC_PIV_link_unsecure first. */
  if (link->application != TC_PIV_APPLICATION_PIV ||
      (link->flags & (TC_PIV_LINK_SECURED | TC_PIV_LINK_SM_LOST)))
    return TC_PIV_REFUSED;
  /* A new establishment ends a session bound earlier (Part 2 4.3). */
  tc_piv_link_unbind(link);
  link->flags &= (uint8_t)~(TC_PIV_LINK_VCI | TC_PIV_LINK_PIN_VERIFIED);
  TC_PIV_SM_handshake handshake;
  if (TC_PIV_SM_begin(session, suite, host_id, random, &handshake, workspace) != TC_OK)
    return request_fail(link, session, workspace, response, 0, TC_PIV_ERROR);
  uint8_t data[REQUEST_MAX_BYTES];
  /* CLA 00 INS 87 P1 suite P2 04 and Le 00, always plain (4.1.8). */
  const TC_APDU_command command = {
      {data, request_write(&handshake, data)}, TC_APDU_SHORT_MAX_NE, TC_PIV_PLAIN_CLA,
      TC_PIV_INS_GENERAL_AUTHENTICATE,         (uint8_t)suite,       KEY_ESTABLISHMENT};
  TC_APDU_response answer;
  const TC_PIV_result result = tc_piv_link_transceive(link, TC_PIV_COMMAND_GENERAL_AUTHENTICATE,
                                                      &command, response, &answer);
  if (result != TC_PIV_OK)
    return request_fail(link, session, workspace, response, 0, result);
  if (answer.sw != TC_PIV_SW_SUCCESS_VALUE)
    return request_fail(link, session, workspace, response, answer.sw, TC_PIV_CARD_STATUS);
  TC_PIV_SM_peer decoded;
  if (!peer_read(answer.data, suite, &decoded))
    return request_fail(link, session, workspace, response, 0, TC_PIV_INVALID);
  link->security = &tc_piv_sm_security;
  link->sm = session;
  *peer = decoded;
  return TC_PIV_OK;
}
#endif
