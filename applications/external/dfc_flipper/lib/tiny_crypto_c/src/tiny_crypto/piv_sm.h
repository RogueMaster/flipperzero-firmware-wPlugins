/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_SM_H_
#define TINY_CRYPTO_PIV_SM_H_
#include <tiny_crypto/aes_dynamic.h>
#include <tiny_crypto/ec.h>

#if TC_PIV_SM_ENABLE_CS7
#define TC_PIV_SM_KEY_BYTES 32
#define TC_PIV_SM_COORDINATE_BYTES 48
#else
#define TC_PIV_SM_KEY_BYTES 16
#define TC_PIV_SM_COORDINATE_BYTES 32
#endif

#define TC_PIV_SM_AUTHENTICATED_SPANS_MAX 8

typedef enum { TC_PIV_SM_CS2 = 0x27, TC_PIV_SM_CS7 = 0x2e } TC_PIV_SM_suite;
typedef enum {
  TC_PIV_SM_IDLE = 0, TC_PIV_SM_ESTABLISHING, TC_PIV_SM_READY, TC_PIV_SM_PENDING
} TC_PIV_SM_state;

/* Zero-initialize before first use. Treat members as private; copying a live
 * session would reuse keys and counters. Clear when the card is removed or
 * transport delivery becomes uncertain. */
typedef struct {
  union {
    struct {
      uint8_t scalar[TC_PIV_SM_COORDINATE_BYTES];
      uint8_t public_key[1 + 2 * TC_PIV_SM_COORDINATE_BYTES], host_id[8];
    } handshake;
    struct {
      uint8_t mac_key[TC_PIV_SM_KEY_BYTES], enc_key[TC_PIV_SM_KEY_BYTES], rmac_key[TC_PIV_SM_KEY_BYTES];
      uint8_t counter[16], command_mcv[16], response_mcv[16];
    } traffic;
  } data;
  uint8_t suite, state;
} TC_PIV_SM;

typedef struct {
  union {
    TC_EC_workspace ec;
    struct {
      union { TC_AES_dynamic_key aes; TC_AES_dynamic_CMAC cmac; } cipher;
      uint8_t material[4 * TC_PIV_SM_KEY_BYTES], digest[32], block[16];
    } symmetric;
  } operation;
  uint8_t secret[TC_PIV_SM_COORDINATE_BYTES];
} TC_PIV_SM_workspace;

/* Fields emitted by session setup. Spans borrow session storage and remain
 * valid until the session changes or is cleared. Protocol encoders decide how
 * these fields are carried. */
typedef struct {
  TC_bytes host_identifier, public_key;
  TC_PIV_SM_suite suite;
} TC_PIV_SM_handshake;

/* Peer fields decoded by the protocol layer. certificate is the
 * exact encoded CVC used by the key-derivation transcript. */
typedef struct {
  TC_bytes certificate, nonce, cryptogram;
} TC_PIV_SM_peer;

/* authenticated is an ordered list of already-framed bytes. It may include
 * the ciphertext output span so a protocol layer can authenticate its encoded
 * header, ciphertext and trailing fields without copying. */
typedef struct {
  TC_bytes plaintext;
  uint8_t* ciphertext;
  size_t ciphertext_capacity;
  const TC_bytes* authenticated;
  size_t authenticated_count;
} TC_PIV_SM_protect_request;

typedef struct {
  TC_bytes ciphertext, tag;
  const TC_bytes* authenticated;
  size_t authenticated_count;
} TC_PIV_SM_unprotect_request;

#ifdef __cplusplus
extern "C" {
#endif

/* Keep writable objects disjoint from each other and from inputs, except for
 * protect's ciphertext spans. Argument/capacity failures preserve outputs.
 * Used workspace is wiped. Sessions retain no input pointers. */

/* Wipe session keys and counters and return to IDLE. Accepts NULL. */
void TC_PIV_SM_clear(TC_PIV_SM* session);

/* Starts a new session and returns the fields needed by a protocol handshake.
 * Valid arguments discard any previous session. A failed RNG or 16 rejected
 * scalars leaves the session cleared. */
TC_status TC_PIV_SM_begin(TC_PIV_SM* session, TC_PIV_SM_suite suite,
    const uint8_t host_id[8], TC_random_fn random, void* random_user,
    TC_PIV_SM_handshake* handshake, TC_PIV_SM_workspace* workspace);

/* Authenticate the peer key through the application's trust workflow, then
 * pass that key and the unchanged decoded peer fields here. A key-confirmation
 * mismatch returns TC_MISMATCH and clears the session. */
TC_status TC_PIV_SM_finish(TC_PIV_SM* session, const TC_PIV_SM_peer* peer,
    TC_bytes authenticated_key, TC_PIV_SM_workspace* workspace);

/* Return the padded ciphertext size for plaintext_length, or TC_ERROR on
 * overflow. Empty plaintext has an empty ciphertext. */
TC_status TC_PIV_SM_ciphertext_size(size_t plaintext_length, size_t* ciphertext_length);

/* Encrypt plaintext and authenticate the supplied ordered spans. The caller
 * owns all protocol framing and places the ciphertext span in authenticated
 * where its protocol requires it. Only one protected request may be pending.
 * Ciphertext storage may overlap authenticated spans; keep plaintext separate. */
TC_status TC_PIV_SM_protect(TC_PIV_SM* session,
    const TC_PIV_SM_protect_request* request, size_t* ciphertext_length,
    uint8_t tag[8], TC_PIV_SM_workspace* workspace);

/* Authenticate ordered response spans, then decrypt and check padding.
 * Plaintext is released only after authentication. Insufficient capacity
 * leaves the request pending so the same response can be retried. */
TC_status TC_PIV_SM_unprotect(TC_PIV_SM* session,
    const TC_PIV_SM_unprotect_request* request, uint8_t* plaintext,
    size_t capacity, size_t* plaintext_length, TC_PIV_SM_workspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
