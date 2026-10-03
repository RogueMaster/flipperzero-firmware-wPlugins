/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Shared hash and HMAC operations (FIPS 180-4, RFC 1321, FIPS 198-1). The
 * per-algorithm files supply compression functions and descriptors. */
#include "hash_core_internal.h"

#if TC_HASH_CORE_ENABLED
#include <string.h>
#include "internal.h"

/* Streaming arguments: a live context and a data or digest span that does not
 * overlap it or wrap the address space. */
static inline int tc_hash_update_args(const void* ctx, size_t ctx_len, const uint8_t* data,
                                      size_t data_len)
{
  return ctx != NULL && tc_internal_span_valid(data, data_len) &&
         data_len <= UINTPTR_MAX - (uintptr_t)data &&
         tc_internal_ranges_disjoint(ctx, ctx_len, data, data_len);
}

static inline int tc_hash_final_args(const void* ctx, size_t ctx_len, const uint8_t* digest,
                                     size_t digest_len)
{
  return ctx != NULL && digest != NULL && digest_len <= UINTPTR_MAX - (uintptr_t)digest &&
         tc_internal_ranges_disjoint(ctx, ctx_len, digest, digest_len);
}

#define HMAC_IPAD 0x36u
#define HMAC_OPAD 0x5Cu

/* Public entry points receive descriptors from TC_HASH_INFO_STORAGE. AVR
 * builds copy the descriptor into local RAM, and other builds use it in
 * place. Internal helpers take the loaded descriptor. */
static const tc_hash_algorithm_info* load_info(const tc_hash_algorithm_info* stored,
                                               tc_hash_algorithm_info* local)
{
#if defined(__AVR__) && TC_AVR_PROGMEM
  memcpy_P(local, stored, sizeof *local);
  return local;
#else
  /* Descriptors are addressable in place, so local is unused. */
  (void)local;
  return stored;
#endif
}

const tc_hash_algorithm_info* tc_hash_core_lookup(TC_hash_algorithm algorithm)
{
  switch (algorithm) {
#if TC_ENABLE_SHA1
  case TC_HASH_SHA1:
    return &tc_sha1_info;
#endif
#if TC_ENABLE_SHA224
  case TC_HASH_SHA224:
    return &tc_sha224_info;
#endif
#if TC_ENABLE_SHA256
  case TC_HASH_SHA256:
    return &tc_sha256_info;
#endif
#if TC_ENABLE_SHA384
  case TC_HASH_SHA384:
    return &tc_sha384_info;
#endif
#if TC_ENABLE_SHA512
  case TC_HASH_SHA512:
    return &tc_sha512_info;
#endif
  default:
    return NULL;
  }
}

size_t tc_hash_core_digest_bytes(const tc_hash_algorithm_info* stored)
{
  tc_hash_algorithm_info local;
  return load_info(stored, &local)->digest_bytes;
}

void tc_hash_store_be32_words(uint8_t* digest, const void* state, size_t words)
{
  const uint32_t* value = (const uint32_t*)state;
  size_t i;
  for (i = 0; i < words; ++i)
    tc_internal_store_be32(digest + 4u * i, value[i]);
}

void tc_hash_store_be64_words(uint8_t* digest, const void* state, size_t words)
{
  const uint64_t* value = (const uint64_t*)state;
  size_t i;
  for (i = 0; i < words; ++i)
    tc_internal_store_be64(digest + 8u * i, value[i]);
}

void tc_hash_store_le32_words(uint8_t* digest, const void* state, size_t words)
{
  const uint32_t* value = (const uint32_t*)state;
  size_t i, byte;
  for (i = 0; i < words; ++i)
    for (byte = 0; byte < 4; ++byte)
      digest[4u * i + byte] = (uint8_t)(value[i] >> (8u * byte));
}

/*****************************************************************************/
/* Hash operations on a view                                                 */
/*****************************************************************************/

/* A context is usable only between a successful init and its final. A
 * partial-block count at or past the block size marks a corrupted context. */
static int view_is_live(const tc_hash_algorithm_info* info, tc_hash_view view)
{
  return *view.active == 1 && *view.used < info->block_bytes;
}

static void view_start(const tc_hash_algorithm_info* info, tc_hash_view view)
{
  TC_secure_zero(view.context, view.size);
  info->state_init(view.state);
  *view.active = 1;
}

static TC_status view_absorb(const tc_hash_algorithm_info* info, tc_hash_view view,
                             const uint8_t* data, size_t length)
{
  if (length == 0)
    return TC_OK;
  if (*view.count > info->max_message_bytes ||
      (uint64_t)length > info->max_message_bytes - *view.count)
    return TC_ERROR;
  *view.count += (uint64_t)length;
  tc_hash_stream_absorb(view.state, view.used, view.buffer, data, length, info->block_bytes,
                        info->compress);
  return TC_OK;
}

/* Pad, compress the tail and serialize the digest. The view stays live, and
 * the caller ends or wipes it. */
static void view_finish(const tc_hash_algorithm_info* info, tc_hash_view view, uint8_t* digest)
{
  tc_hash_stream_finish(view.state, *view.count, view.used, view.buffer, info->block_bytes,
                        info->length_bytes, (tc_hash_length_encoding)info->length_encoding,
                        info->compress);
  info->digest_out(view.state, digest);
}

/* Final and clear consume the context by wiping it. The zeroed active flag
 * makes reuse fail until the next init. */
static void view_end(tc_hash_view view)
{
  TC_secure_zero(view.context, view.size);
}

TC_status tc_hash_core_init(const tc_hash_algorithm_info* stored, void* context)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  if (context == NULL)
    return TC_ERROR;
  view_start(info, info->view(context));
  return TC_OK;
}

TC_status tc_hash_core_update(const tc_hash_algorithm_info* stored, void* context,
                              const uint8_t* data, size_t length)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hash_view view;
  if (context == NULL)
    return TC_ERROR;
  view = info->view(context);
  if (!tc_hash_update_args(view.context, view.size, data, length) || !view_is_live(info, view))
    return TC_ERROR;
  return view_absorb(info, view, data, length);
}

TC_status tc_hash_core_update_parts(const tc_hash_algorithm_info* stored, void* context,
                                    const TC_bytes* parts, size_t count)
{
  TC_status status = TC_OK;
  if (context == NULL || (count != 0 && parts == NULL))
    return TC_ERROR;
  for (size_t i = 0; i < count; ++i)
    if (!tc_internal_span_valid(parts[i].data, parts[i].length))
      return TC_ERROR;
  for (size_t i = 0; status == TC_OK && i < count; ++i)
    status = tc_hash_core_update(stored, context, parts[i].data, parts[i].length);
  return status;
}

TC_status tc_hash_core_final(const tc_hash_algorithm_info* stored, void* context, uint8_t* digest)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hash_view view;
  if (context == NULL)
    return TC_ERROR;
  view = info->view(context);
  if (!tc_hash_final_args(view.context, view.size, digest, info->digest_bytes) ||
      !view_is_live(info, view))
    return TC_ERROR;
  view_finish(info, view, digest);
  view_end(view);
  return TC_OK;
}

void tc_hash_core_clear(const tc_hash_algorithm_info* stored, void* context)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hash_view view;
  if (context == NULL)
    return;
  view = info->view(context);
  TC_secure_zero(view.context, view.size);
}

TC_status tc_hash_core_digest(const tc_hash_algorithm_info* stored, void* workspace, TC_bytes data,
                              uint8_t* digest)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hash_view view = info->view(workspace);
  TC_status status;
  if (digest == NULL || (data.length != 0 && data.data == NULL))
    return TC_ERROR;
  view_start(info, view);
  status = view_absorb(info, view, data.data, data.length);
  if (status == TC_OK)
    view_finish(info, view, digest);
  TC_secure_zero(view.context, view.size);
  return status;
}

/*****************************************************************************/
/* HMAC (FIPS 198-1 / RFC 2104)                                              */
/*****************************************************************************/

#if TC_ENABLE_HMAC

/* Derive K0 into block. A key longer than one block is hashed with the inner
 * context, which saves a second hash context on the stack. HMAC setup
 * rewrites the inner context next. */
static TC_status hmac_key_block(const tc_hash_algorithm_info* info, tc_hash_view inner,
                                const uint8_t* key, size_t key_length, uint8_t* block)
{
  memset(block, 0, info->block_bytes);
  if (key_length > info->block_bytes) {
    view_start(info, inner);
    if (view_absorb(info, inner, key, key_length) != TC_OK)
      return TC_ERROR;
    view_finish(info, inner, block);
  } else if (key_length != 0) {
    memcpy(block, key, key_length);
  }
  return TC_OK;
}

TC_status tc_hmac_core_init(const tc_hash_algorithm_info* stored, void* context, const uint8_t* key,
                            size_t key_length)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  uint8_t block[TC_HASH_CORE_MAX_BLOCK];
  tc_hmac_view hmac;
  size_t i;
  TC_status status;

  if (context == NULL)
    return TC_ERROR;
  hmac = info->hmac_view(context);
  /* The key is read after the context is wiped, so it must lie outside it.
   * Every failure leaves the context wiped and inactive. */
  const int key_valid =
      key_length == 0 ||
      (key != NULL && tc_internal_ranges_disjoint(hmac.context, hmac.size, key, key_length));
  TC_secure_zero(hmac.context, hmac.size);
  if (!key_valid)
    return TC_ERROR;
  status = hmac_key_block(info, hmac.inner, key, key_length, block);
  if (status == TC_OK) {
    /* Inner hash: H((K0 ^ ipad) || message). */
    for (i = 0; i < info->block_bytes; ++i)
      block[i] ^= HMAC_IPAD;
    view_start(info, hmac.inner);
    info->compress(hmac.inner.state, block);
    *hmac.inner.count = info->block_bytes;

    /* Outer hash state after (K0 ^ opad). XOR with ipad ^ opad converts the block in place. */
    for (i = 0; i < info->block_bytes; ++i)
      block[i] ^= (uint8_t)(HMAC_IPAD ^ HMAC_OPAD);
    info->state_init(hmac.outer_state);
    info->compress(hmac.outer_state, block);
  } else {
    TC_secure_zero(hmac.context, hmac.size);
  }
  TC_secure_zero(block, sizeof block);
  return status;
}

TC_status tc_hmac_core_update(const tc_hash_algorithm_info* stored, void* context,
                              const uint8_t* data, size_t length)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hmac_view hmac;
  if (context == NULL)
    return TC_ERROR;
  hmac = info->hmac_view(context);
  if (!tc_hash_update_args(hmac.context, hmac.size, data, length) ||
      !view_is_live(info, hmac.inner))
    return TC_ERROR;
  return view_absorb(info, hmac.inner, data, length);
}

TC_status tc_hmac_core_final(const tc_hash_algorithm_info* stored, void* context, uint8_t* tag)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  uint8_t inner_digest[TC_HASH_CORE_MAX_DIGEST];
  uint8_t block[TC_HASH_CORE_MAX_BLOCK];
  uint8_t used;
  tc_hmac_view hmac;

  if (context == NULL)
    return TC_ERROR;
  hmac = info->hmac_view(context);
  if (!tc_hash_final_args(hmac.context, hmac.size, tag, info->digest_bytes) ||
      !view_is_live(info, hmac.inner))
    return TC_ERROR;
  view_finish(info, hmac.inner, inner_digest);

  /* Outer hash: continue from the stored opad state with the inner digest as
   * the only remaining message block. */
  memcpy(block, inner_digest, info->digest_bytes);
  used = info->digest_bytes;
  tc_hash_stream_finish(hmac.outer_state, (uint64_t)info->block_bytes + info->digest_bytes, &used,
                        block, info->block_bytes, info->length_bytes,
                        (tc_hash_length_encoding)info->length_encoding, info->compress);
  info->digest_out(hmac.outer_state, tag);

  TC_secure_zero(inner_digest, sizeof inner_digest);
  TC_secure_zero(block, sizeof block);
  TC_secure_zero(hmac.context, hmac.size);
  return TC_OK;
}

TC_status tc_hmac_core_parts(const tc_hash_algorithm_info* info, void* context, const uint8_t* key,
                             size_t key_length, const TC_bytes* parts, size_t count, uint8_t* tag)
{
  uint8_t full[TC_HASH_CORE_MAX_DIGEST];
  TC_status status = tc_hmac_core_init(info, context, key, key_length);
  for (size_t i = 0; status == TC_OK && i < count; ++i)
    status = tc_hmac_core_update(info, context, parts[i].data, parts[i].length);
  if (status == TC_OK)
    status = tc_hmac_core_final(info, context, full);
  if (status == TC_OK)
    memcpy(tag, full, tc_hash_core_digest_bytes(info));
  tc_hmac_core_clear(info, context);
  TC_secure_zero(full, sizeof full);
  return status;
}

TC_status tc_hmac_core_resume_parts(const tc_hash_algorithm_info* stored, const void* keyed,
                                    void* context, const TC_bytes* parts, size_t count,
                                    uint8_t* tag)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  uint8_t full[TC_HASH_CORE_MAX_DIGEST];
  if (keyed == NULL || context == NULL)
    return TC_ERROR;
  memcpy(context, keyed, info->hmac_view(context).size);
  TC_status status = TC_OK;
  for (size_t i = 0; status == TC_OK && i < count; ++i)
    status = tc_hmac_core_update(stored, context, parts[i].data, parts[i].length);
  if (status == TC_OK)
    status = tc_hmac_core_final(stored, context, full);
  if (status == TC_OK)
    memcpy(tag, full, info->digest_bytes);
  tc_hmac_core_clear(stored, context);
  TC_secure_zero(full, sizeof full);
  return status;
}

void tc_hmac_core_clear(const tc_hash_algorithm_info* stored, void* context)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  tc_hmac_view hmac;
  if (context == NULL)
    return;
  hmac = info->hmac_view(context);
  TC_secure_zero(hmac.context, hmac.size);
}

/* One-shot HMAC with the library-wide default or explicit short-tag policy. */
TC_status tc_hmac_core_digest(const tc_hash_algorithm_info* stored, void* workspace, TC_bytes key,
                              TC_bytes message, TC_buffer tag, int short_tag)
{
  tc_hash_algorithm_info local;
  const tc_hash_algorithm_info* info = load_info(stored, &local);
  uint8_t full[TC_HASH_CORE_MAX_DIGEST];
  TC_status status;

  /* SP 800-107: a truncated tag keeps the leftmost bytes. The algorithm and
   * library-wide policies jointly set the default minimum. */
  const size_t minimum =
      TC_HMAC_MIN_TAG_LEN > TC_MIN_TAG_LEN ? TC_HMAC_MIN_TAG_LEN : TC_MIN_TAG_LEN;
  if (tag.data == NULL || tag.capacity > info->digest_bytes ||
      (short_tag ? tag.capacity == 0 || tag.capacity >= minimum : tag.capacity < minimum) ||
      (message.length != 0 && message.data == NULL))
    return TC_ERROR;
  status = tc_hmac_core_init(stored, workspace, key.data, key.length);
  if (status == TC_OK)
    status = tc_hmac_core_update(stored, workspace, message.data, message.length);
  if (status == TC_OK)
    status = tc_hmac_core_final(stored, workspace, full);
  if (status == TC_OK)
    memcpy(tag.data, full, tag.capacity);
  tc_hmac_core_clear(stored, workspace);
  TC_secure_zero(full, sizeof full);
  return status;
}

TC_status tc_hmac_core_verify(const tc_hash_algorithm_info* stored, void* workspace, TC_bytes key,
                              TC_bytes message, TC_bytes tag, int short_tag)
{
  uint8_t computed[TC_HASH_CORE_MAX_DIGEST];
  TC_status status;
  if (tag.data == NULL)
    return TC_ERROR;
  status = tc_hmac_core_digest(stored, workspace, key, message, (TC_buffer){computed, tag.length},
                               short_tag);
  return tc_internal_verify_tag(status, computed, sizeof computed, tag.data, tag.length);
}

#endif /* TC_ENABLE_HMAC */

#endif /* TC_HASH_CORE_ENABLED */
