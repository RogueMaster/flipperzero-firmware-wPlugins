/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Client-side PIV secure messaging: key establishment, key confirmation and
 * command and response protection for cipher suites 2 and 7.
 * Standards: SP 800-73-5 Part 2 section 4, SP 800-56A Rev. 3, SP 800-38B.
 * Configuration: TC_ENABLE_PIV_SM, TC_PIV_SM_ENABLE_CS2 and
 * TC_PIV_SM_ENABLE_CS7.
 * Limitations: APDU framing, chaining and status words are in
 * piv_sm_apdu.h. CVC authentication is in piv_sm_authenticate.h.
 * Contracts: docs/api.md. Guide: docs/piv-sm.md. */
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
/* IDLE holds no session. ESTABLISHING follows begin and awaits finish. READY
 * accepts one protect call. PENDING awaits the response to that command. */
typedef enum {
  TC_PIV_SM_IDLE = 0,
  TC_PIV_SM_ESTABLISHING,
  TC_PIV_SM_READY,
  TC_PIV_SM_PENDING
} TC_PIV_SM_state;

/* Zero-initialize before first use. Treat members as private and read the
 * state with TC_PIV_SM_get_state. Never copy a live session, because the copy
 * would reuse keys and counters. Clear when the card is removed or transport
 * delivery becomes uncertain. */
typedef struct {
  union {
    struct {
      uint8_t scalar[TC_PIV_SM_COORDINATE_BYTES];
      uint8_t public_key[1 + 2 * TC_PIV_SM_COORDINATE_BYTES], host_id[8];
    } handshake;
    struct {
      uint8_t mac_key[TC_PIV_SM_KEY_BYTES], enc_key[TC_PIV_SM_KEY_BYTES],
          rmac_key[TC_PIV_SM_KEY_BYTES];
      uint8_t counter[16], command_mcv[16], response_mcv[16], peer_digest[32];
    } traffic;
  } data;
  uint8_t suite, state;
} TC_PIV_SM;

typedef struct {
  union {
    TC_EC_workspace ec;
    struct {
      union {
        TC_AES_dynamic_key aes;
        TC_AES_dynamic_CMAC cmac;
      } cipher;
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

/* Peer fields decoded from the card's key establishment response
 * (SP 800-73-5 Part 2 section 4.1, step C11). certificate is the exact encoded
 * CVC used by the key-derivation transcript. card_control is the received
 * CB_ICC byte. Pass it unchanged so finish can check it and bind it into
 * OtherInfo. */
typedef struct {
  TC_bytes certificate, nonce, cryptogram;
  uint8_t card_control;
} TC_PIV_SM_peer;

/* authenticated is an ordered list of already-framed bytes. It may include
 * the ciphertext output span so a protocol layer can authenticate its encoded
 * header, ciphertext and trailing fields without copying. */
typedef struct {
  TC_bytes plaintext;
  TC_buffer ciphertext;
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
 * protect's ciphertext spans and unprotect's exact in-place alias. Sessions
 * retain no input pointers.
 *
 * The status-returning functions return TC_ERROR for NULL, overlapping or
 * malformed arguments and for a call made in the wrong state. Those argument
 * errors leave the session, workspace and outputs unchanged. Causes listed
 * under a function as after validation instead end the session. Every other
 * return wipes the used workspace and leaves the session in the state named
 * for that result. These functions take no work budget. Their EC steps run
 * under the exact TC_EC_operation_work cost of one operation. */

#if TC_ENABLE_PIV_SM
/* Wipe session keys and counters and return to IDLE (SP 800-73-5 Part 2
 * section 4.3). Accepts NULL. */
void TC_PIV_SM_clear(TC_PIV_SM* session);

/* Return the session state. NULL reports TC_PIV_SM_IDLE. Callers use it to
 * tell a retryable unprotect result from one that ended the session. */
TC_PIV_SM_state TC_PIV_SM_get_state(const TC_PIV_SM* session);

/* Start a new session and return the fields needed by a protocol handshake
 * (SP 800-73-5 Part 2 section 4.1.1). random fills the ephemeral scalar. Any
 * state is accepted. Valid arguments discard any previous session. A suite
 * that is unknown or disabled in this build is an argument error.
 * TC_OK: ESTABLISHING. handshake borrows session storage.
 * TC_ERROR after validation: a failed RNG or 16 rejected scalars leaves the
 * session IDLE and handshake unchanged. */
TC_status TC_PIV_SM_begin(TC_PIV_SM* session, TC_PIV_SM_suite suite, const uint8_t host_id[8],
                          TC_random_source random, TC_PIV_SM_handshake* handshake,
                          TC_PIV_SM_workspace* workspace);

/* Authenticate the peer key through the application's trust workflow, then
 * pass that key and the unchanged decoded peer fields here. finish runs ECDH,
 * the key derivation and the key confirmation of SP 800-73-5 Part 2 sections
 * 4.1.6 and 4.1.7. Requires ESTABLISHING.
 * TC_OK: READY with fresh session keys.
 * TC_MISMATCH: the key-confirmation cryptogram differs. IDLE.
 * TC_ERROR after validation: IDLE. Causes are a nonzero card_control, missing
 * peer fields, peer field or key lengths that differ from the suite, an
 * invalid peer key and KDF failure. */
TC_status TC_PIV_SM_finish(TC_PIV_SM* session, const TC_PIV_SM_peer* peer,
                           TC_bytes authenticated_key, TC_PIV_SM_workspace* workspace);

/* 1 when READY or PENDING and certificate is the exact CVC bound into the
 * authenticated key-establishment transcript, else 0. */
int TC_PIV_SM_peer_matches(const TC_PIV_SM* session, TC_bytes certificate);

/* Store the padded ciphertext size for plaintext_length. Padding always adds
 * 1 to 16 bytes, so a multiple of 16 grows by a full block. Empty plaintext
 * has an empty ciphertext. Returns TC_ERROR for a NULL output or when the size
 * exceeds SIZE_MAX, leaving *ciphertext_length unchanged. */
TC_status TC_PIV_SM_ciphertext_size(size_t plaintext_length, size_t* ciphertext_length);

/* Encrypt plaintext and authenticate the supplied ordered spans (SP 800-73-5
 * Part 2 sections 4.2.2 to 4.2.4). The caller owns all protocol framing and
 * places the ciphertext span in authenticated where its protocol requires it.
 * Ciphertext storage may overlap authenticated spans. Keep plaintext
 * separate. Requires READY, so only one protected request
 * may be pending. A ciphertext.capacity below TC_PIV_SM_ciphertext_size is an
 * argument error.
 * TC_OK: PENDING. Writes ciphertext, *ciphertext_length and tag.
 * TC_ERROR after validation: IDLE with ciphertext wiped. Causes are an
 * exhausted message counter and cipher failure. */
TC_status TC_PIV_SM_protect(TC_PIV_SM* session, const TC_PIV_SM_protect_request* request,
                            size_t* ciphertext_length, TC_buffer tag,
                            TC_PIV_SM_workspace* workspace);

/* Authenticate ordered response spans, then decrypt and check padding
 * (SP 800-73-5 Part 2 sections 4.2.5 and 4.2.6).
 * Plaintext is released only after authentication. Requires PENDING.
 * plaintext is disjoint from every input, or exactly request->ciphertext.data
 * with a capacity of at most request->ciphertext.length. That exact alias
 * decrypts in place and overwrites the ciphertext and the authenticated span
 * holding it. Every other overlap is an argument error.
 * TC_OK: READY. Writes plaintext and *plaintext_length.
 * TC_MISMATCH: the response tag differs. IDLE.
 * TC_ERROR with state PENDING: an argument error, or capacity below the
 * authenticated plaintext length. The session and outputs are unchanged, so
 * the same response can be retried with corrected arguments or a larger
 * buffer. A capacity of request->ciphertext.length always suffices.
 * TC_ERROR with state IDLE: malformed padding, an exhausted counter or cipher
 * failure. *plaintext_length is unchanged and written plaintext is wiped. A
 * padding failure is found before any plaintext is written, so in place the
 * ciphertext stays unchanged. */
TC_status TC_PIV_SM_unprotect(TC_PIV_SM* session, const TC_PIV_SM_unprotect_request* request,
                              TC_buffer plaintext, size_t* plaintext_length,
                              TC_PIV_SM_workspace* workspace);
#endif

#ifdef __cplusplus
}
#endif
#endif
