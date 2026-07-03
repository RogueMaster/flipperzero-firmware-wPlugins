/*
---------------------------------------------------------------------------
Copyright (c) 1998-2010, Brian Gladman, Worcester, UK. All rights reserved.

The redistribution and use of this software (with or without changes)
is allowed without the payment of fees or royalties provided that:

  source code distributions include the above copyright notice, this
  list of conditions and the following disclaimer;

  binary distributions include the above copyright notice, this list
  of conditions and the following disclaimer in their documentation.

This software is provided 'as is' with no explicit or implied warranties
in respect of its operation, including, but not limited to, correctness
and fitness for purpose.
---------------------------------------------------------------------------
Issue Date: 11/01/2011

 I am grateful for the work done by Mark Rodenkirch and Jason Papadopoulos
 in helping to remove a bug in the operation of this code on big endian
 systems when fast buffer operations are enabled.
 ---------------------------------------------------------------------------
*/

#ifndef _GCM_H
#define _GCM_H

#include "aes.h"
#include "gf128mul.h"

/*  USER DEFINABLE OPTIONS (Further options need to be set in gf128mul.h) */

/*  UNIT_BITS sets the size of variables used to process 16 byte buffers
    when the buffer alignment allows this.  When buffers are processed
    in bytes, 16 individual operations are invoolved.  But if, say, such
    a buffer is divided into 4 32 bit variables, it can then be processed
    in 4 operations, making the code typically much faster. In general
    it will pay to use the longest natively supported size, which will
    probably be 32 or 64 bits in 32 and 64 bit systems respectively.
*/

#if defined(UNIT_BITS)
#undef UNIT_BITS
#endif

#if !defined(UNIT_BITS)
#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN
#if 0
#define UNIT_BITS 8
#elif 0
#define UNIT_BITS 32
#elif 1
#define UNIT_BITS 64
#endif
#elif defined(_WIN64)
#define UNIT_BITS 64
#else
#define UNIT_BITS 32
#endif
#endif

#if UNIT_BITS == 64 && !defined(NEED_UINT_64T)
#define NEED_UINT_64T
#endif

/* END OF USER DEFINABLE OPTIONS */

/*  After encryption or decryption operations the return value of
    'compute tag' will be one of the values RETURN_GOOD, RETURN_WARN
    or RETURN_ERROR, the latter indicating an error. A return value
    RETURN_GOOD indicates that both encryption and authentication
    have taken place and resulted in the returned tag value. If
    the returned value is RETURN_WARN, the tag value is the result
    of authentication alone without encryption (CCM) or decryption
    (GCM and EAX).
*/
#ifndef RETURN_GOOD
#define RETURN_WARN  1
#define RETURN_GOOD  0
#define RETURN_ERROR -1
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef RET_TYPE_DEFINED
typedef int ret_type;
#endif
UNIT_TYPEDEF(gcm_unit_t, UNIT_BITS);
BUFR_TYPEDEF(gcm_buf_t, UNIT_BITS, AES_BLOCK_SIZE);

#define GCM_BLOCK_SIZE AES_BLOCK_SIZE

/* The GCM-AES  context  */

typedef struct {
#if defined(TABLES_64K)
    gf_t64k_a gf_t64k;
#endif
#if defined(TABLES_8K)
    gf_t8k_a gf_t8k;
#endif
#if defined(TABLES_4K)
    gf_t4k_a gf_t4k;
#endif
#if defined(TABLES_256)
    gf_t256_a gf_t256;
#endif
    gcm_buf_t ctr_val; /* CTR counter value            */
    gcm_buf_t enc_ctr; /* encrypted CTR block          */
    gcm_buf_t hdr_ghv; /* ghash buffer (header)        */
    gcm_buf_t txt_ghv; /* ghash buffer (ciphertext)    */
    gf_t ghash_h; /* ghash H value                */
    aes_encrypt_ctx aes[1]; /* AES encryption context       */
    uint32_t y0_val; /* initial counter value        */
    uint32_t hdr_cnt; /* header bytes so far          */
    uint32_t txt_ccnt; /* text bytes so far (encrypt)  */
    uint32_t txt_acnt; /* text bytes so far (auth)     */
} gcm_ctx;

/* The following calls handle mode initialisation, keying and completion    */

ret_type gcm_init_and_key(/* initialise mode and set key  */
                          const unsigned char key[], /* the key value                */
                          gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_end(/* clean up and end operation   */
                 gcm_ctx ctx[1]); /* the mode context             */

/* The following calls handle complete messages in memory as one operation  */

ret_type gcm_encrypt_message(/* encrypt an entire message    */
                             const unsigned char iv[], /* the initialisation vector    */
                             unsigned long iv_len, /* and its length in bytes      */
                             const unsigned char hdr[], /* the header buffer            */
                             unsigned long hdr_len, /* and its length in bytes      */
                             unsigned char msg[], /* the message buffer           */
                             unsigned long msg_len, /* and its length in bytes      */
                             unsigned char tag[], /* the buffer for the tag       */
                             unsigned long tag_len, /* and its length in bytes      */
                             gcm_ctx ctx[1]); /* the mode context             */

/* RETURN_GOOD is returned if the input tag */
/* matches that for the decrypted message   */
ret_type gcm_decrypt_message(/* decrypt an entire message    */
                             const unsigned char iv[], /* the initialisation vector    */
                             unsigned long iv_len, /* and its length in bytes      */
                             const unsigned char hdr[], /* the header buffer            */
                             unsigned long hdr_len, /* and its length in bytes      */
                             unsigned char msg[], /* the message buffer           */
                             unsigned long msg_len, /* and its length in bytes      */
                             const unsigned char tag[], /* the buffer for the tag       */
                             unsigned long tag_len, /* and its length in bytes      */
                             gcm_ctx ctx[1]); /* the mode context             */

/* The following calls handle messages in a sequence of operations followed */
/* by tag computation after the sequence has been completed. In these calls */
/* the user is responsible for verfiying the computed tag on decryption     */

ret_type gcm_init_message(/* initialise a new message     */
                          const unsigned char iv[], /* the initialisation vector    */
                          unsigned long iv_len, /* and its length in bytes      */
                          gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_auth_header(/* authenticate the header      */
                         const unsigned char hdr[], /* the header buffer            */
                         unsigned long hdr_len, /* and its length in bytes      */
                         gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_encrypt(/* encrypt & authenticate data  */
                     unsigned char data[], /* the data buffer              */
                     unsigned long data_len, /* and its length in bytes      */
                     gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_decrypt(/* authenticate & decrypt data  */
                     unsigned char data[], /* the data buffer              */
                     unsigned long data_len, /* and its length in bytes      */
                     gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_compute_tag(/* compute authentication tag   */
                         unsigned char tag[], /* the buffer for the tag       */
                         unsigned long tag_len, /* and its length in bytes      */
                         gcm_ctx ctx[1]); /* the mode context             */

/*  The use of the following calls should be avoided if possible because
    their use requires a very good understanding of the way this encryption
    mode works and the way in which this code implements it in order to use
    them correctly.

    The gcm_auth_data routine is used to authenticate encrypted message data.
    In message encryption gcm_crypt_data must be called before gcm_auth_data
    is called since it is encrypted data that is authenticated.  In message
    decryption authentication must occur before decryption and data can be
    authenticated without being decrypted if necessary.

    If these calls are used it is up to the user to ensure that these routines
    are called in the correct order and that the correct data is passed to
    them.

    When gcm_compute_tag is called it is assumed that an error in use has
    occurred if both encryption (or decryption) and authentication have taken
    place but the total lengths of the message data respectively authenticated
    and encrypted are not the same. If authentication has taken place but
    there has been no corresponding encryption or decryption operations (none
    at all) only a warning is issued. This should be treated as an error if it
    occurs during encryption but it is only signalled as a warning as it might
    be intentional when decryption operations are involved (this avoids having
    different compute tag functions for encryption and decryption). Decryption
    operations can be undertaken freely after authetication but if the tag is
    computed after such operations an error will be signalled if the lengths
    of the data authenticated and decrypted don't match.
*/

ret_type gcm_auth_data(/* authenticate ciphertext data */
                       const unsigned char data[], /* the data buffer              */
                       unsigned long data_len, /* and its length in bytes      */
                       gcm_ctx ctx[1]); /* the mode context             */

ret_type gcm_crypt_data(/* encrypt or decrypt data      */
                        unsigned char data[], /* the data buffer              */
                        unsigned long data_len, /* and its length in bytes      */
                        gcm_ctx ctx[1]); /* the mode context             */

#if defined(__cplusplus)
}
#endif

#endif
