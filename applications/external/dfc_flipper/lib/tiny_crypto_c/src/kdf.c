/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * NIST SP 800-108r1 KBKDF for tiny_crypto_c: counter, feedback and
 * double-pipeline mode over the HMAC and CMAC PRFs compiled into the profile.
 *
 * One generic core drives every mode. A PRF is a tagged, typed descriptor:
 * HMAC PRFs name a hash-core descriptor, and CMAC PRFs name their cipher's
 * CMAC functions. Per-PRF wrappers provide typed context storage with no heap
 * allocation. The MAC reads each block as an ordered list of borrowed segments
 * (counter, chaining value, fixed input), which bounds the stack to two
 * contexts of the selected MAC plus two h-byte blocks.
 */

#include <string.h>
#include <tiny_crypto/kdf.h>
#include "internal.h"

#if TC_ENABLE_KDF
#include "hash_core_internal.h"

/* A later PRF block may reread each input after output bytes are written. */
static inline int tc_kdf_output_disjoint(const uint8_t* output, size_t output_len,
                                         const uint8_t* input, size_t input_len)
{
  return input_len == 0 || tc_internal_ranges_disjoint(output, output_len, input, input_len);
}

#define TC_KDF_HAVE_CMAC (TC_KBKDF_HAVE_AES_CMAC || TC_KBKDF_HAVE_DES_CMAC)

#if TC_KDF_HAVE_CMAC
struct tc_kdf_cmac {
  int (*key_ok)(size_t key_len);
  TC_status (*init)(void* ctx, const uint8_t* key, size_t key_len);
  TC_status (*update)(void* ctx, const uint8_t* data, size_t len);
  TC_status (*final)(void* ctx, uint8_t* out);
  void (*clear)(void* ctx);
};
#endif

enum tc_kdf_prf_kind { TC_KDF_PRF_HMAC, TC_KDF_PRF_CMAC };

/* Typed wrappers own the matching context storage. The core copies only
 * ctx_size bytes and reads only the selected descriptor arm. */
struct tc_kdf_prf {
  enum tc_kdf_prf_kind kind;
  uint8_t out_len;
  size_t ctx_size;
  union {
#if TC_KBKDF_HAVE_HMAC
    const tc_hash_algorithm_info* hash;
#endif
#if TC_KDF_HAVE_CMAC
    const struct tc_kdf_cmac* cmac;
#endif
  } mac;
};

#if TC_KBKDF_HAVE_HMAC
static inline struct tc_kdf_prf tc_kdf_hmac_prf(const tc_hash_algorithm_info* hash, size_t ctx_size,
                                                uint8_t out_len)
{
  struct tc_kdf_prf prf;
  prf.kind = TC_KDF_PRF_HMAC;
  prf.out_len = out_len;
  prf.ctx_size = ctx_size;
  prf.mac.hash = hash;
  return prf;
}
#endif

/* config.h requires at least one PRF family with KDF. Each dispatch below
 * tests for CMAC first, so an HMAC-only build compiles the HMAC arm alone. In
 * a CMAC-only build every PRF is CMAC, so the trailing HMAC fallback is
 * unreachable. */
static inline int tc_kdf_key_ok(const struct tc_kdf_prf* prf, size_t key_len)
{
#if TC_KDF_HAVE_CMAC
  if (prf->kind == TC_KDF_PRF_CMAC)
    return prf->mac.cmac->key_ok(key_len);
#endif
  /* HMAC accepts any nonzero key length (RFC 2104 section 2). */
  return prf->kind == TC_KDF_PRF_HMAC && key_len != 0;
}

static inline TC_status tc_kdf_mac_init(const struct tc_kdf_prf* prf, void* ctx, const uint8_t* key,
                                        size_t key_len)
{
#if TC_KDF_HAVE_CMAC
  if (prf->kind == TC_KDF_PRF_CMAC)
    return prf->mac.cmac->init(ctx, key, key_len);
#endif
#if TC_KBKDF_HAVE_HMAC
  return tc_hmac_core_init(prf->mac.hash, ctx, key, key_len);
#else
  return TC_ERROR;
#endif
}

static inline TC_status tc_kdf_mac_update(const struct tc_kdf_prf* prf, void* ctx,
                                          const uint8_t* data, size_t len)
{
#if TC_KDF_HAVE_CMAC
  if (prf->kind == TC_KDF_PRF_CMAC)
    return prf->mac.cmac->update(ctx, data, len);
#endif
#if TC_KBKDF_HAVE_HMAC
  return tc_hmac_core_update(prf->mac.hash, ctx, data, len);
#else
  return TC_ERROR;
#endif
}

static inline TC_status tc_kdf_mac_final(const struct tc_kdf_prf* prf, void* ctx, uint8_t* out)
{
#if TC_KDF_HAVE_CMAC
  if (prf->kind == TC_KDF_PRF_CMAC)
    return prf->mac.cmac->final(ctx, out);
#endif
#if TC_KBKDF_HAVE_HMAC
  return tc_hmac_core_final(prf->mac.hash, ctx, out);
#else
  return TC_ERROR;
#endif
}

static inline void tc_kdf_mac_clear(const struct tc_kdf_prf* prf, void* ctx)
{
#if TC_KDF_HAVE_CMAC
  if (prf->kind == TC_KDF_PRF_CMAC) {
    prf->mac.cmac->clear(ctx);
    return;
  }
#endif
#if TC_KBKDF_HAVE_HMAC
  tc_hmac_core_clear(prf->mac.hash, ctx);
#else
  TC_secure_zero(ctx, prf->ctx_size);
#endif
}

/* Begin from a cached keyed context and feed each segment in order. The
 * HMAC and CMAC PRFs share this one loop through the descriptor, so the HMAC
 * arm does not route through tc_hmac_core_resume_parts. */
static inline TC_status tc_kdf_prf_run(const struct tc_kdf_prf* prf, const void* initialized,
                                       void* ctx, const TC_bytes* segments, unsigned count,
                                       uint8_t* block)
{
  unsigned i;
  memcpy(ctx, initialized, prf->ctx_size);
  for (i = 0; i < count; ++i) {
    if (segments[i].length != 0 &&
        tc_kdf_mac_update(prf, ctx, segments[i].data, segments[i].length) != TC_OK) {
      tc_kdf_mac_clear(prf, ctx);
      return TC_ERROR;
    }
  }
  return tc_kdf_mac_final(prf, ctx, block);
}

/*****************************************************************************/
/* PRF descriptors                                                           */
/*****************************************************************************/

#if TC_KBKDF_HAVE_AES_CMAC
/* The AES key size is fixed per build, so the KDK must match it exactly. */
static int tc_kdf_aes_key_ok(size_t key_len)
{
  return key_len == TC_AES_KEYLEN;
}
static TC_status tc_kdf_aes_cmac_init(void* ctx, const uint8_t* key, size_t key_len)
{
  /* key_ok checked the length. The PRF init type passes it for DES keys. */
  (void)key_len;
  return TC_AES_CMAC_init((struct TC_AES_CMAC_ctx*)ctx, (TC_bytes){key, key_len});
}
static TC_status tc_kdf_aes_cmac_update(void* ctx, const uint8_t* data, size_t len)
{
  return TC_AES_CMAC_update((struct TC_AES_CMAC_ctx*)ctx, (TC_bytes){data, len});
}
static TC_status tc_kdf_aes_cmac_final(void* ctx, uint8_t* out)
{
  return TC_AES_CMAC_final((struct TC_AES_CMAC_ctx*)ctx, (TC_buffer){out, TC_AES_CMAC_TAG_MAX});
}
static void tc_kdf_aes_cmac_clear(void* ctx)
{
  TC_AES_CMAC_ctx_clear((struct TC_AES_CMAC_ctx*)ctx);
}
static const struct tc_kdf_cmac tc_kdf_aes_cmac = {tc_kdf_aes_key_ok, tc_kdf_aes_cmac_init,
                                                   tc_kdf_aes_cmac_update, tc_kdf_aes_cmac_final,
                                                   tc_kdf_aes_cmac_clear};
static const struct tc_kdf_prf tc_kdf_prf_aes_cmac = {TC_KDF_PRF_CMAC,
                                                      TC_AES_CMAC_TAG_MAX,
                                                      sizeof(struct TC_AES_CMAC_ctx),
                                                      {.cmac = &tc_kdf_aes_cmac}};
#endif /* TC_KBKDF_HAVE_AES_CMAC */

#if TC_KBKDF_HAVE_DES_CMAC
static int tc_kdf_des_key_ok(size_t key_len)
{
  return key_len == 8 || key_len == 16 || key_len == 24;
}
static TC_status tc_kdf_des_cmac_init(void* ctx, const uint8_t* key, size_t key_len)
{
  return TC_DES_CMAC_init((struct TC_DES_CMAC_ctx*)ctx, (TC_bytes){key, key_len});
}
static TC_status tc_kdf_des_cmac_update(void* ctx, const uint8_t* data, size_t len)
{
  return TC_DES_CMAC_update((struct TC_DES_CMAC_ctx*)ctx, (TC_bytes){data, len});
}
static TC_status tc_kdf_des_cmac_final(void* ctx, uint8_t* out)
{
  return TC_DES_CMAC_final((struct TC_DES_CMAC_ctx*)ctx, (TC_buffer){out, TC_DES_CMAC_TAG_MAX});
}
static void tc_kdf_des_cmac_clear(void* ctx)
{
  TC_DES_CMAC_ctx_clear((struct TC_DES_CMAC_ctx*)ctx);
}
static const struct tc_kdf_cmac tc_kdf_des_cmac = {tc_kdf_des_key_ok, tc_kdf_des_cmac_init,
                                                   tc_kdf_des_cmac_update, tc_kdf_des_cmac_final,
                                                   tc_kdf_des_cmac_clear};
static const struct tc_kdf_prf tc_kdf_prf_des_cmac = {TC_KDF_PRF_CMAC,
                                                      TC_DES_CMAC_TAG_MAX,
                                                      sizeof(struct TC_DES_CMAC_ctx),
                                                      {.cmac = &tc_kdf_des_cmac}};
#endif /* TC_KBKDF_HAVE_DES_CMAC */

/*****************************************************************************/
/* Shared derivation core                                                    */
/*****************************************************************************/

#define TC_KDF_MODE_COUNTER 0
#define TC_KDF_MODE_FEEDBACK 1
#define TC_KDF_MODE_PIPELINE 2

static int tc_kdf_counter_bits_ok(unsigned bits)
{
  return bits == TC_KBKDF_COUNTER_8 || bits == TC_KBKDF_COUNTER_16 || bits == TC_KBKDF_COUNTER_24 ||
         bits == TC_KBKDF_COUNTER_32;
}

/* Caller-owned PRF storage for one derivation. initialized and ctx each hold
 * one PRF context. chain and block hold one PRF output. chain is NULL in
 * counter mode. */
struct tc_kdf_state {
  void* initialized;
  void* ctx;
  uint8_t* chain;
  uint8_t* block;
};

/*
 * input1 / input2 depend on the mode:
 *   counter   : input1 = data before the counter, input2 = data after the counter
 *   feedback  : input1 = IV (K(0)),               input2 = fixed input
 *   pipeline  : input1 unused,                     input2 = fixed input (= A(0))
 * Exactly output.capacity bytes are derived.
 */
static TC_status tc_kdf_derive(const struct tc_kdf_prf* prf, int mode, TC_bytes kdk,
                               const struct TC_KBKDF_params* params, TC_bytes input1,
                               TC_bytes input2, TC_buffer output, const struct tc_kdf_state* state)
{
  const uint8_t* key = kdk.data;
  const size_t key_len = kdk.length;
  const uint8_t* in1 = input1.data;
  const size_t in1_len = input1.length;
  const uint8_t* in2 = input2.data;
  const size_t in2_len = input2.length;
  uint8_t* out = output.data;
  const size_t out_len = output.capacity;
  void* initialized = state->initialized;
  void* ctx = state->ctx;
  uint8_t* chain = state->chain;
  uint8_t* block = state->block;
  uint8_t ctr[4];
  TC_bytes seg[3];
  const uint8_t* chain_p = NULL;
  size_t chain_n = 0;
  size_t h, reps, pos;
  unsigned r = 0, ctr_len = 0;
  int use_ctr;
  uint32_t i;

  if (key == NULL || key_len == 0 || !tc_kdf_key_ok(prf, key_len) || params == NULL ||
      out == NULL || out_len == 0 || !tc_internal_span_valid(in1, in1_len) ||
      !tc_internal_span_valid(in2, in2_len))
    return TC_ERROR;
  if (!tc_kdf_output_disjoint(out, out_len, key, key_len) ||
      !tc_kdf_output_disjoint(out, out_len, in1, in1_len) ||
      !tc_kdf_output_disjoint(out, out_len, in2, in2_len))
    return TC_ERROR;

  use_ctr = (mode == TC_KDF_MODE_COUNTER) ? 1 : (params->use_counter != 0);
  if (use_ctr) {
    r = params->counter_bits;
    if (!tc_kdf_counter_bits_ok(r))
      return TC_ERROR;
    ctr_len = r / 8u;
    if (mode != TC_KDF_MODE_COUNTER && (params->counter_location < TC_KBKDF_CTR_BEFORE_ITER ||
                                        params->counter_location > TC_KBKDF_CTR_AFTER_FIXED))
      return TC_ERROR;
  }

  /* n = ceil(L / h) must fit the counter and the 32-bit loop index. Keep the
     shift explicitly 32-bit: unsigned int is only 16 bits on AVR. */
  h = prf->out_len;
  reps = out_len / h + (out_len % h != 0);
#if SIZE_MAX > UINT32_MAX
  if (reps > UINT32_MAX)
    return TC_ERROR;
#endif
  if (use_ctr && r < 32 && reps > (((uint32_t)1u << r) - 1u))
    return TC_ERROR;

  /* Cache the keyed PRF state once. Each block starts from a small context
     copy, avoiding repeated HMAC key hashing and CMAC key expansion. */
  if (tc_kdf_mac_init(prf, initialized, key, key_len) != TC_OK)
    return TC_ERROR;

  if (mode == TC_KDF_MODE_FEEDBACK) {
    chain_p = in1;
    chain_n = in1_len;
  } else if (mode == TC_KDF_MODE_PIPELINE) {
    chain_p = in2;
    chain_n = in2_len;
  }

  pos = 0;
  for (i = 1; pos < out_len; ++i) {
    unsigned count;
    size_t take;

    if (use_ctr) {
      unsigned k;
      for (k = 0; k < ctr_len; ++k)
        ctr[k] = (uint8_t)(i >> (8u * (ctr_len - 1u - k)));
    }

    if (mode == TC_KDF_MODE_PIPELINE) {
      /* A(i) = PRF(KDK, A(i-1)). A(0) is the fixed input itself. */
      seg[0].data = chain_p;
      seg[0].length = chain_n;
      if (tc_kdf_prf_run(prf, initialized, ctx, seg, 1, chain) != TC_OK)
        goto fail;
      chain_p = chain;
      chain_n = h;
    }

    if (mode == TC_KDF_MODE_COUNTER) {
      seg[0].data = in1;
      seg[0].length = in1_len;
      seg[1].data = ctr;
      seg[1].length = ctr_len;
      seg[2].data = in2;
      seg[2].length = in2_len;
      count = 3;
    } else if (!use_ctr) {
      seg[0].data = chain_p;
      seg[0].length = chain_n;
      seg[1].data = in2;
      seg[1].length = in2_len;
      count = 2;
    } else if (params->counter_location == TC_KBKDF_CTR_BEFORE_ITER) {
      seg[0].data = ctr;
      seg[0].length = ctr_len;
      seg[1].data = chain_p;
      seg[1].length = chain_n;
      seg[2].data = in2;
      seg[2].length = in2_len;
      count = 3;
    } else if (params->counter_location == TC_KBKDF_CTR_AFTER_ITER) {
      seg[0].data = chain_p;
      seg[0].length = chain_n;
      seg[1].data = ctr;
      seg[1].length = ctr_len;
      seg[2].data = in2;
      seg[2].length = in2_len;
      count = 3;
    } else /* TC_KBKDF_CTR_AFTER_FIXED */
    {
      seg[0].data = chain_p;
      seg[0].length = chain_n;
      seg[1].data = in2;
      seg[1].length = in2_len;
      seg[2].data = ctr;
      seg[2].length = ctr_len;
      count = 3;
    }

    if (tc_kdf_prf_run(prf, initialized, ctx, seg, count, block) != TC_OK)
      goto fail;

    if (mode == TC_KDF_MODE_FEEDBACK) {
      memcpy(chain, block, h);
      chain_p = chain;
      chain_n = h;
    }

    take = out_len - pos;
    if (take > h)
      take = h;
    memcpy(out + pos, block, take);
    pos += take;
  }

  /* The keyed contexts hold the KDK's ipad/opad state; wipe them on success
   * as on failure. */
  tc_kdf_mac_clear(prf, initialized);
  tc_kdf_mac_clear(prf, ctx);
  if (chain != NULL)
    TC_secure_zero(chain, h);
  TC_secure_zero(block, h);
  return TC_OK;

fail:
  tc_kdf_mac_clear(prf, initialized);
  tc_kdf_mac_clear(prf, ctx);
  TC_secure_zero(out, out_len);
  if (chain != NULL)
    TC_secure_zero(chain, h);
  TC_secure_zero(block, h);
  return TC_ERROR;
}

/*****************************************************************************/
/* Fixed-input helper                                                        */
/*****************************************************************************/

TC_status TC_KBKDF_fixed_input(TC_bytes label_span, TC_bytes context_span, size_t out_len,
                               TC_buffer output)
{
  const uint8_t* label = label_span.data;
  const size_t label_len = label_span.length;
  const uint8_t* context = context_span.data;
  const size_t context_len = context_span.length;
  uint8_t* buf = output.data;
  size_t needed;

  if (buf == NULL || !tc_internal_span_valid(label, label_len) ||
      !tc_internal_span_valid(context, context_len))
    return TC_ERROR;
  /* [L]_32 is a bit count, so out_len must stay below 2^29 bytes. */
  if (out_len == 0 || out_len > 0x1FFFFFFFu)
    return TC_ERROR;
  if (label_len > SIZE_MAX - 5u || context_len > SIZE_MAX - 5u - label_len)
    return TC_ERROR;
  needed = TC_KBKDF_FIXED_INPUT_LEN(label_len, context_len);
  if (output.capacity < needed)
    return TC_ERROR;
  if (!tc_kdf_output_disjoint(buf, needed, label, label_len) ||
      !tc_kdf_output_disjoint(buf, needed, context, context_len))
    return TC_ERROR;
  if (label_len != 0 && memchr(label, 0, label_len) != NULL)
    return TC_ERROR;

  if (label_len != 0)
    memcpy(buf, label, label_len);
  buf[label_len] = 0x00;
  if (context_len != 0)
    memcpy(buf + label_len + 1u, context, context_len);
  tc_internal_store_be32(buf + label_len + 1u + context_len, (uint32_t)out_len << 3);
  return TC_OK;
}

/*****************************************************************************/
/* Public per-PRF entry points                                               */
/*****************************************************************************/

/* Typed storage keeps unrelated enabled PRFs out of this call's stack budget. */
#define TC_KDF_DEFINE_FAMILY(NAME, PRF, CTX, DIGESTLEN)                                            \
  static TC_status tc_kdf_##NAME(int mode, TC_bytes key, const struct TC_KBKDF_params* params,     \
                                 TC_bytes input1, TC_bytes input2, TC_buffer out)                  \
  {                                                                                                \
    CTX initialized, ctx;                                                                          \
    const struct tc_kdf_prf prf = PRF;                                                             \
    uint8_t chain[DIGESTLEN], block[DIGESTLEN];                                                    \
    const struct tc_kdf_state state = {&initialized, &ctx, chain, block};                          \
    return tc_kdf_derive(&prf, mode, key, params, input1, input2, out, &state);                    \
  }                                                                                                \
  /* Counter mode has no chaining value, so it needs no chain storage. */                          \
  TC_status TC_KBKDF_##NAME##_counter(TC_bytes key, const struct TC_KBKDF_params* params,          \
                                      TC_bytes before, TC_bytes after, TC_buffer out)              \
  {                                                                                                \
    CTX initialized, ctx;                                                                          \
    const struct tc_kdf_prf prf = PRF;                                                             \
    uint8_t block[DIGESTLEN];                                                                      \
    const struct tc_kdf_state state = {&initialized, &ctx, NULL, block};                           \
    return tc_kdf_derive(&prf, TC_KDF_MODE_COUNTER, key, params, before, after, out, &state);      \
  }                                                                                                \
  TC_status TC_KBKDF_##NAME##_feedback(TC_bytes key, const struct TC_KBKDF_params* params,         \
                                       TC_bytes iv, TC_bytes fixed, TC_buffer out)                 \
  {                                                                                                \
    return tc_kdf_##NAME(TC_KDF_MODE_FEEDBACK, key, params, iv, fixed, out);                       \
  }                                                                                                \
  TC_status TC_KBKDF_##NAME##_pipeline(TC_bytes key, const struct TC_KBKDF_params* params,         \
                                       TC_bytes fixed, TC_buffer out)                              \
  {                                                                                                \
    return tc_kdf_##NAME(TC_KDF_MODE_PIPELINE, key, params, (TC_bytes){NULL, 0}, fixed, out);      \
  }

#if TC_KBKDF_HAVE_HMAC_SHA1
TC_KDF_DEFINE_FAMILY(HMAC_SHA1,
                     tc_kdf_hmac_prf(&tc_sha1_info, sizeof(struct TC_HMAC_SHA1_ctx),
                                     TC_SHA1_DIGESTLEN),
                     struct TC_HMAC_SHA1_ctx, TC_SHA1_DIGESTLEN)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA224
TC_KDF_DEFINE_FAMILY(HMAC_SHA224,
                     tc_kdf_hmac_prf(&tc_sha224_info, sizeof(struct TC_HMAC_SHA224_ctx),
                                     TC_SHA224_DIGESTLEN),
                     struct TC_HMAC_SHA224_ctx, TC_SHA224_DIGESTLEN)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA256
TC_KDF_DEFINE_FAMILY(HMAC_SHA256,
                     tc_kdf_hmac_prf(&tc_sha256_info, sizeof(struct TC_HMAC_SHA256_ctx),
                                     TC_SHA256_DIGESTLEN),
                     struct TC_HMAC_SHA256_ctx, TC_SHA256_DIGESTLEN)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA384
TC_KDF_DEFINE_FAMILY(HMAC_SHA384,
                     tc_kdf_hmac_prf(&tc_sha384_info, sizeof(struct TC_HMAC_SHA384_ctx),
                                     TC_SHA384_DIGESTLEN),
                     struct TC_HMAC_SHA384_ctx, TC_SHA384_DIGESTLEN)
#endif
#if TC_KBKDF_HAVE_HMAC_SHA512
TC_KDF_DEFINE_FAMILY(HMAC_SHA512,
                     tc_kdf_hmac_prf(&tc_sha512_info, sizeof(struct TC_HMAC_SHA512_ctx),
                                     TC_SHA512_DIGESTLEN),
                     struct TC_HMAC_SHA512_ctx, TC_SHA512_DIGESTLEN)
#endif
#if TC_KBKDF_HAVE_AES_CMAC
TC_KDF_DEFINE_FAMILY(AES_CMAC, tc_kdf_prf_aes_cmac, struct TC_AES_CMAC_ctx, TC_AES_CMAC_TAG_MAX)
#endif
#if TC_KBKDF_HAVE_DES_CMAC
TC_KDF_DEFINE_FAMILY(DES_CMAC, tc_kdf_prf_des_cmac, struct TC_DES_CMAC_ctx, TC_DES_CMAC_TAG_MAX)
#endif

#endif /* TC_ENABLE_KDF */
