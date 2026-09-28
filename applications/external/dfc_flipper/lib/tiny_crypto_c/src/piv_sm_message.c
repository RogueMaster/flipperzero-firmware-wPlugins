/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_sm_internal.h"
#if TC_ENABLE_PIV_SM

static int counter_nonzero(const uint8_t counter[16])
{
  uint8_t value = 0;
  size_t i;
  for (i = 0; i < 16; ++i) value |= counter[i];
  return value != 0;
}

/* Derive the CBC IV. Responses use the preceding command's counter. */
static TC_status prepare_cipher(TC_PIV_SM* session, const tc_sm_suite* suite,
                                 TC_PIV_SM_workspace* w, int response)
{
  uint8_t* iv = TC_SM_SYM(w).material;
  size_t i;
  memcpy(iv,session->data.traffic.counter,16);
  if (response) {
    for (i = 16; i > 0; --i) if (iv[i - 1]-- != 0) break;
    iv[0] = 0x80;
  }
  if (TC_AES_dynamic_key_init(&TC_SM_SYM(w).cipher.aes,
      session->data.traffic.enc_key,suite->key_bytes) != TC_OK) return TC_ERROR;
  return TC_AES_dynamic_encrypt(&TC_SM_SYM(w).cipher.aes,iv);
}

static int spans_valid(const TC_bytes* spans, size_t count)
{
  size_t i;
  if (!spans && count) return 0;
  for (i = 0; i < count; ++i)
    if (!spans[i].data && spans[i].length) return 0;
  return 1;
}

static int span_contains(TC_bytes outer, TC_bytes inner)
{
  uintptr_t start, contained;
  size_t offset;
  if (!inner.length) return 1;
  if (!outer.data || outer.length < inner.length) return 0;
  start = (uintptr_t)outer.data;
  contained = (uintptr_t)inner.data;
  if (contained < start) return 0;
  offset = (size_t)(contained - start);
  return offset <= outer.length && inner.length <= outer.length - offset;
}

static int spans_contain(const TC_bytes* spans, size_t count, TC_bytes required)
{
  size_t i;
  for (i = 0; i < count; ++i)
    if (span_contains(spans[i],required)) return 1;
  return !required.length;
}

TC_status TC_PIV_SM_ciphertext_size(size_t plaintext_length, size_t* ciphertext_length)
{
  size_t padding;
  if (!ciphertext_length) return TC_ERROR;
  if (!plaintext_length) {
    *ciphertext_length = 0;
    return TC_OK;
  }
  padding = 16 - plaintext_length % 16;
  if (plaintext_length > SIZE_MAX - padding) return TC_ERROR;
  *ciphertext_length = plaintext_length + padding;
  return TC_OK;
}

TC_status TC_PIV_SM_protect(TC_PIV_SM* session,
    const TC_PIV_SM_protect_request* request, size_t* ciphertext_length,
    uint8_t tag[8], TC_PIV_SM_workspace* workspace)
{
  const tc_sm_suite* suite;
  size_t needed = 0;
  TC_status status = TC_ERROR;
  if (!session || !request || !ciphertext_length || !tag || !workspace ||
      ((!request->plaintext.data) && request->plaintext.length) ||
      ((!request->ciphertext) && request->ciphertext_capacity) ||
      request->authenticated_count > TC_PIV_SM_AUTHENTICATED_SPANS_MAX ||
      !spans_valid(request->authenticated,request->authenticated_count) ||
      session->state != TC_PIV_SM_READY ||
      TC_PIV_SM_ciphertext_size(request->plaintext.length,&needed) != TC_OK ||
      request->ciphertext_capacity < needed ||
      !spans_contain(request->authenticated,request->authenticated_count,
        (TC_bytes){request->ciphertext,needed})) return TC_ERROR;
  {
    const TC_bytes writable[] = {{(const uint8_t*)session,sizeof *session},
      {(const uint8_t*)workspace,sizeof *workspace},
      {request->ciphertext,request->ciphertext_capacity},
      {(const uint8_t*)ciphertext_length,sizeof *ciphertext_length},{tag,8}};
    const TC_bytes input[] = {{(const uint8_t*)request,sizeof *request},request->plaintext,
      {(const uint8_t*)request->authenticated,
       request->authenticated_count * sizeof *request->authenticated}};
    if (!tc_sm_disjoint(writable,5,input,3)) return TC_ERROR;
    if (!tc_sm_disjoint(writable,2,request->authenticated,
                        request->authenticated_count) ||
        !tc_sm_disjoint(writable + 3,2,request->authenticated,
                        request->authenticated_count)) return TC_ERROR;
  }
  suite = tc_sm_suite_get(session->suite);
  /* Responses replace the high counter byte with 80. Stop before the low 120
   * bits repeat, even though the request counter is 128 bits. */
  if (!suite || session->data.traffic.counter[0] != 0 ||
      !counter_nonzero(session->data.traffic.counter)) {
    TC_PIV_SM_clear(session);
    return TC_ERROR;
  }
  if (needed) {
    memcpy(request->ciphertext,request->plaintext.data,request->plaintext.length);
    request->ciphertext[request->plaintext.length] = 0x80;
    memset(request->ciphertext + request->plaintext.length + 1,0,
           needed - request->plaintext.length - 1);
    if (prepare_cipher(session,suite,workspace,0) != TC_OK ||
        TC_AES_dynamic_CBC_encrypt(&TC_SM_SYM(workspace).cipher.aes,
          TC_SM_SYM(workspace).material,request->ciphertext,needed) != TC_OK) goto done;
    TC_AES_dynamic_key_clear(&TC_SM_SYM(workspace).cipher.aes);
  }
  {
    TC_AES_dynamic_CMAC* mac = &TC_SM_SYM(workspace).cipher.cmac;
    size_t i;
    status = TC_AES_dynamic_CMAC_init(mac,session->data.traffic.mac_key,suite->key_bytes);
    if (status == TC_OK) status = TC_AES_dynamic_CMAC_update(mac,
      session->data.traffic.command_mcv,16);
    for (i = 0; i < request->authenticated_count && status == TC_OK; ++i)
      status = TC_AES_dynamic_CMAC_update(mac,request->authenticated[i].data,
                                          request->authenticated[i].length);
    if (status == TC_OK) status = TC_AES_dynamic_CMAC_final(mac,TC_SM_SYM(workspace).digest);
    TC_AES_dynamic_CMAC_clear(mac);
    if (status != TC_OK) goto done;
  }
  memcpy(tag,TC_SM_SYM(workspace).digest,8);
  memcpy(session->data.traffic.command_mcv,TC_SM_SYM(workspace).digest,16);
  tc_internal_increment_be(session->data.traffic.counter,16);
  session->state = TC_PIV_SM_PENDING;
  *ciphertext_length = needed;
  status = TC_OK;
done:
  if (status != TC_OK) {
    if (request->ciphertext && needed) TC_secure_zero(request->ciphertext,needed);
    TC_PIV_SM_clear(session);
  }
  TC_secure_zero(workspace,sizeof *workspace);
  return status;
}

/* Scan the full final block for ISO 7816 padding; return zero if malformed. */
static size_t padding_length(const uint8_t block[16])
{
  unsigned seen = 0, bad = 0, count = 0, i;
  for (i = 0; i < 16; ++i) {
    unsigned value = block[15 - i], zero = value == 0, marker = value == 0x80;
    unsigned before = seen ^ 1u;
    bad |= before & (zero ^ 1u) & (marker ^ 1u);
    count += before;
    seen |= marker;
  }
  return seen && !bad ? count : 0;
}

TC_status TC_PIV_SM_unprotect(TC_PIV_SM* session,
    const TC_PIV_SM_unprotect_request* request, uint8_t* plaintext,
    size_t capacity, size_t* plaintext_length, TC_PIV_SM_workspace* workspace)
{
  const tc_sm_suite* suite;
  size_t length = 0, offset, padding;
  TC_status status = TC_ERROR;
  int preserve_session = 0;
  if (!session || !request || !plaintext_length || !workspace ||
      ((!request->ciphertext.data) && request->ciphertext.length) ||
      !request->tag.data || request->tag.length != 8 ||
      request->authenticated_count > TC_PIV_SM_AUTHENTICATED_SPANS_MAX ||
      !spans_valid(request->authenticated,request->authenticated_count) ||
      ((!plaintext) && capacity) || request->ciphertext.length % 16 ||
      !spans_contain(request->authenticated,request->authenticated_count,
        request->ciphertext) ||
      session->state != TC_PIV_SM_PENDING) return TC_ERROR;
  {
    const TC_bytes writable[] = {{(const uint8_t*)session,sizeof *session},
      {(const uint8_t*)workspace,sizeof *workspace},{plaintext,capacity},
      {(const uint8_t*)plaintext_length,sizeof *plaintext_length}};
    const TC_bytes input[] = {{(const uint8_t*)request,sizeof *request},request->ciphertext,
      request->tag,{(const uint8_t*)request->authenticated,
      request->authenticated_count * sizeof *request->authenticated}};
    if (!tc_sm_disjoint(writable,4,input,4)) return TC_ERROR;
    if (!tc_sm_disjoint(writable,4,request->authenticated,
                        request->authenticated_count)) return TC_ERROR;
  }
  suite = tc_sm_suite_get(session->suite);
  if (!suite || !counter_nonzero(session->data.traffic.counter)) goto done;
  {
    TC_AES_dynamic_CMAC* mac = &TC_SM_SYM(workspace).cipher.cmac;
    size_t i;
    status = TC_AES_dynamic_CMAC_init(mac,session->data.traffic.rmac_key,suite->key_bytes);
    if (status == TC_OK) status = TC_AES_dynamic_CMAC_update(mac,
      session->data.traffic.response_mcv,16);
    for (i = 0; i < request->authenticated_count && status == TC_OK; ++i)
      status = TC_AES_dynamic_CMAC_update(mac,request->authenticated[i].data,
                                          request->authenticated[i].length);
    if (status == TC_OK) status = TC_AES_dynamic_CMAC_final(mac,TC_SM_SYM(workspace).digest);
    TC_AES_dynamic_CMAC_clear(mac);
    if (status != TC_OK) goto done;
  }
  status = TC_ct_equal(TC_SM_SYM(workspace).digest,request->tag.data,8);
  if (status != TC_OK) goto done;
  status = TC_ERROR;
  if (request->ciphertext.length) {
    const uint8_t* previous;
    if (prepare_cipher(session,suite,workspace,1) != TC_OK) goto done;
    memcpy(TC_SM_SYM(workspace).block,
           request->ciphertext.data + request->ciphertext.length - 16,16);
    if (TC_AES_dynamic_decrypt(&TC_SM_SYM(workspace).cipher.aes,
        TC_SM_SYM(workspace).block) != TC_OK) goto done;
    previous = request->ciphertext.length == 16 ? TC_SM_SYM(workspace).material :
      request->ciphertext.data + request->ciphertext.length - 32;
    tc_internal_xor(TC_SM_SYM(workspace).block,previous,16);
    padding = padding_length(TC_SM_SYM(workspace).block);
    if (!padding) goto done;
    length = request->ciphertext.length - padding;
  }
  if (capacity < length) { preserve_session = 1; goto done; }
  for (offset = 0; offset < length; offset += 16) {
    size_t take = length - offset;
    if (take > 16) take = 16;
    memcpy(TC_SM_SYM(workspace).block,request->ciphertext.data + offset,16);
    if (TC_AES_dynamic_CBC_decrypt(&TC_SM_SYM(workspace).cipher.aes,
        TC_SM_SYM(workspace).material,TC_SM_SYM(workspace).block,16) != TC_OK) {
      TC_secure_zero(plaintext,length);
      goto done;
    }
    memcpy(plaintext + offset,TC_SM_SYM(workspace).block,take);
  }
  memcpy(session->data.traffic.response_mcv,TC_SM_SYM(workspace).digest,16);
  session->state = TC_PIV_SM_READY;
  *plaintext_length = length;
  status = TC_OK;
done:
  if (status != TC_OK && !preserve_session) TC_PIV_SM_clear(session);
  TC_secure_zero(workspace,sizeof *workspace);
  return status;
}
#endif
