/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_sm_internal.h"
#if TC_ENABLE_PIV_SM
#include <tiny_crypto/hash.h>

const tc_sm_suite* tc_sm_suite_get(unsigned suite)
{
#if TC_PIV_SM_ENABLE_CS2
  static const tc_sm_suite cs2 = {TC_EC_P256, 32, 16, 16, {4, 9, 9, 9, 9, 8}, TC_SSKDF_SHA256};
  if (suite == TC_PIV_SM_CS2)
    return &cs2;
#endif
#if TC_PIV_SM_ENABLE_CS7
  static const tc_sm_suite cs7 = {TC_EC_P384, 48, 32, 24, {4, 13, 13, 13, 13, 8}, TC_SSKDF_SHA384};
  if (suite == TC_PIV_SM_CS7)
    return &cs7;
#endif
  return NULL;
}

int tc_sm_disjoint(const TC_bytes* writable, size_t count, const TC_bytes* input, size_t inputs)
{
  size_t i, j;
  if ((count && !writable) || (inputs && !input))
    return 0;
  for (j = 0; j < inputs; ++j)
    if (!input[j].data && input[j].length)
      return 0;
  for (i = 0; i < count; ++i) {
    if (!writable[i].data && writable[i].length)
      return 0;
    for (j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(writable[i].data, writable[i].length, writable[j].data,
                                       writable[j].length))
        return 0;
    for (j = 0; j < inputs; ++j)
      if (!tc_internal_ranges_disjoint(writable[i].data, writable[i].length, input[j].data,
                                       input[j].length))
        return 0;
  }
  return 1;
}

TC_status tc_sm_mac(TC_PIV_SM_workspace* w, const uint8_t* key, size_t key_len, TC_bytes prefix,
                    const TC_bytes* input, size_t count, uint8_t output[16])
{
  size_t i;
  TC_AES_dynamic_CMAC* mac = &TC_SM_SYM(w).cipher.cmac;
  TC_status status = TC_AES_dynamic_CMAC_init(mac, (TC_bytes){key, key_len});
  if (status == TC_OK && prefix.length)
    status = TC_AES_dynamic_CMAC_update(mac, (TC_bytes){prefix.data, prefix.length});
  for (i = 0; i < count && status == TC_OK; ++i)
    status = TC_AES_dynamic_CMAC_update(mac, (TC_bytes){input[i].data, input[i].length});
  if (status == TC_OK)
    status = TC_AES_dynamic_CMAC_final(mac, (TC_buffer){output, TC_AES_BLOCKLEN});
  TC_AES_dynamic_CMAC_clear(mac);
  return status;
}

void TC_PIV_SM_clear(TC_PIV_SM* session)
{
  if (session)
    TC_secure_zero(session, sizeof *session);
}

TC_PIV_SM_state TC_PIV_SM_get_state(const TC_PIV_SM* session)
{
  return session ? (TC_PIV_SM_state)session->state : TC_PIV_SM_IDLE;
}

TC_status TC_PIV_SM_begin(TC_PIV_SM* session, TC_PIV_SM_suite suite, const uint8_t host_id[8],
                          TC_random_source random, TC_PIV_SM_handshake* handshake,
                          TC_PIV_SM_workspace* workspace)
{
  const tc_sm_suite* settings = tc_sm_suite_get(suite);
  const TC_bytes writable[] = {{(const uint8_t*)session, sizeof *session},
                               {(const uint8_t*)workspace, sizeof *workspace},
                               {(const uint8_t*)handshake, sizeof *handshake}};
  const TC_bytes input[] = {{host_id, 8}};
  size_t public_length;
  unsigned attempt;
  TC_status status = TC_ERROR;
  if (!settings || !random.fill || !tc_sm_disjoint(writable, 3, input, 1))
    return TC_ERROR;
  public_length = 1 + 2 * settings->coordinate_bytes;
  TC_PIV_SM_clear(session);
  for (attempt = 0; attempt < 16; ++attempt) {
    if (random.fill(random.context, session->data.handshake.scalar, settings->coordinate_bytes) !=
        TC_OK)
      break;
    TC_work_budget budget = {TC_EC_operation_work(settings->curve, TC_EC_OPERATION_PUBLIC_KEY)};
    if (TC_EC_public_key(settings->curve,
                         (TC_bytes){session->data.handshake.scalar, settings->coordinate_bytes},
                         (TC_buffer){session->data.handshake.public_key, public_length},
                         &workspace->operation.ec, &budget) == TC_EC_OK) {
      status = TC_OK;
      break;
    }
  }
  if (status == TC_OK) {
    memcpy(session->data.handshake.host_id, host_id, 8);
    session->suite = (uint8_t)suite;
    session->state = TC_PIV_SM_ESTABLISHING;
    handshake->host_identifier = (TC_bytes){session->data.handshake.host_id, 8};
    handshake->public_key = (TC_bytes){session->data.handshake.public_key, public_length};
    handshake->suite = suite;
  } else
    TC_PIV_SM_clear(session);
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

static TC_status finish_response(TC_PIV_SM* session, const TC_PIV_SM_peer* parsed,
                                 TC_bytes authenticated_key, TC_PIV_SM_workspace* workspace)
{
  static const uint8_t host_control[] = {1, 0, 16}, card_control_length = 1;
  static const uint8_t id_length = 8, confirmation[] = {'K', 'C', '_', '1', '_', 'V'};
  const tc_sm_suite* settings;
  TC_status status = TC_ERROR;
  uint8_t nonce_length;
  settings = tc_sm_suite_get(session->suite);
  if (!settings)
    goto done;
  /* SP 800-73-5 Part 2 section 4.1 step H4: CB_ICC must be 0x00. */
  if (parsed->card_control != 0)
    goto done;
  if (!authenticated_key.data || authenticated_key.length != 1 + 2 * settings->coordinate_bytes ||
      parsed->nonce.length != settings->nonce_bytes || parsed->cryptogram.length != 16 ||
      !parsed->certificate.data || !parsed->certificate.length || !parsed->nonce.data ||
      !parsed->cryptogram.data)
    goto done;
  TC_work_budget budget = {TC_EC_operation_work(settings->curve, TC_EC_OPERATION_ECDH)};
  const TC_EC_result agreed = TC_ECDH(
      settings->curve, (TC_bytes){session->data.handshake.scalar, settings->coordinate_bytes},
      authenticated_key, (TC_buffer){workspace->secret, settings->coordinate_bytes},
      &workspace->operation.ec, &budget);
  TC_secure_zero(session->data.handshake.scalar, sizeof session->data.handshake.scalar);
  if (agreed != TC_EC_OK) {
    status = TC_ERROR;
    goto done;
  }
  status = TC_SHA256_digest(parsed->certificate, TC_SM_SYM(workspace).digest);
  if (status != TC_OK)
    goto done;
  nonce_length = (uint8_t)settings->nonce_bytes;
  {
    const TC_bytes info[] = {
        {settings->prefix, 6},     {session->data.handshake.host_id, 8},
        {host_control, 3},         {session->data.handshake.public_key + 1, 16},
        {&id_length, 1},           {TC_SM_SYM(workspace).digest, 8},
        {&nonce_length, 1},        parsed->nonce,
        {&card_control_length, 1}, {&parsed->card_control, 1}};
    /* OtherInfo layout from SP 800-73-5 Part 2 section 4.1.6. */
    status = settings->derive((TC_bytes){workspace->secret, settings->coordinate_bytes}, info,
                              sizeof info / sizeof info[0],
                              (TC_buffer){TC_SM_SYM(workspace).material, 4 * settings->key_bytes});
  }
  TC_secure_zero(workspace->secret, sizeof workspace->secret);
  if (status != TC_OK)
    goto done;
  {
    const TC_bytes mac_input[] = {
        {confirmation, 6},
        {TC_SM_SYM(workspace).digest, 8},
        {session->data.handshake.host_id, 8},
        {session->data.handshake.public_key + 1, 2 * settings->coordinate_bytes}};
    status = tc_sm_mac(workspace, TC_SM_SYM(workspace).material, settings->key_bytes,
                       (TC_bytes){NULL, 0}, mac_input, 4, TC_SM_SYM(workspace).block);
  }
  if (status != TC_OK)
    goto done;
  status = TC_ct_equal((TC_bytes){TC_SM_SYM(workspace).block, 16}, parsed->cryptogram);
  if (status != TC_OK)
    goto done;
  TC_secure_zero(&session->data, sizeof session->data);
  memcpy(session->data.traffic.mac_key, TC_SM_SYM(workspace).material + settings->key_bytes,
         settings->key_bytes);
  memcpy(session->data.traffic.enc_key, TC_SM_SYM(workspace).material + 2 * settings->key_bytes,
         settings->key_bytes);
  memcpy(session->data.traffic.rmac_key, TC_SM_SYM(workspace).material + 3 * settings->key_bytes,
         settings->key_bytes);
  memcpy(session->data.traffic.peer_digest, TC_SM_SYM(workspace).digest,
         sizeof session->data.traffic.peer_digest);
  session->data.traffic.counter[15] = 1;
  session->state = TC_PIV_SM_READY;
done:
  if (status != TC_OK)
    TC_PIV_SM_clear(session);
  TC_secure_zero(workspace, sizeof *workspace);
  return status;
}

int TC_PIV_SM_peer_matches(const TC_PIV_SM* session, TC_bytes certificate)
{
  uint8_t digest[32];
  if (!session || !certificate.data || !certificate.length ||
      (session->state != TC_PIV_SM_READY && session->state != TC_PIV_SM_PENDING) ||
      TC_SHA256_digest(certificate, digest) != TC_OK)
    return 0;
  const int matches = TC_ct_equal((TC_bytes){digest, sizeof digest},
                                  (TC_bytes){session->data.traffic.peer_digest,
                                             sizeof session->data.traffic.peer_digest}) == TC_OK;
  TC_secure_zero(digest, sizeof digest);
  return matches;
}

TC_status TC_PIV_SM_finish(TC_PIV_SM* session, const TC_PIV_SM_peer* peer,
                           TC_bytes authenticated_key, TC_PIV_SM_workspace* workspace)
{
  const TC_bytes writable[] = {{(const uint8_t*)session, sizeof *session},
                               {(const uint8_t*)workspace, sizeof *workspace}};
  if (!peer)
    return TC_ERROR;
  {
    const TC_bytes input[] = {{(const uint8_t*)peer, sizeof *peer},
                              peer->certificate,
                              peer->nonce,
                              peer->cryptogram,
                              authenticated_key};
    if (!session || !workspace || !tc_sm_disjoint(writable, 2, input, 5) ||
        session->state != TC_PIV_SM_ESTABLISHING)
      return TC_ERROR;
  }
  return finish_response(session, peer, authenticated_key, workspace);
}

#endif
