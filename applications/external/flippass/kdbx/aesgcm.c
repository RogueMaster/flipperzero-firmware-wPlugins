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
Issue Date: 30/03/2011

 My thanks to:

   Colin Sinclair for finding an error and suggesting a number of
   improvements to this code.

   John Viega and David McGrew for their support in the development
   of this code and to David for testing it on a big-endIAN system.

   Mark Rodenkirch and Jason Papadopoulos for their help in finding
   a bug in the fast buffer operations on big endian systems.
*/

#include "aesgcm.h"
#include "mode_hdr.h"

/*  This GCM implementation needs a Galois Field multiplier for GF(2^128).
    which operates on field elements using a polynomial field representation
    x^127 + x^126 + ... + x^2 + x + 1 using the bits in a bit sequence that
    will be numbered by the power of x that they represent. GCM uses the
    polynomial x^128 + x^7 + x^2 + x + 1 as its basis for representation.

    The obvious way of representing this in a computer system is to map GF
    'x' to the binary integer '2' - but this was way too obvious for any
    cryptographer to adopt!

    Here bytes are numbered in memory order and  bits within bytes according
    to their integer numeric significance. The term 'little endian' is then
    used to describe mappings in which numeric (power of 2) or field (power
    of x) significance increase with increasing bit or byte numbers with
    'big endian' being used to describe the inverse situation.

    GCM uses little endian byte ordering and big endian bit ordering, a
    representation that will be described as LB. Hence the low end of the
    field polynomial is in byte[0], which has the value 0xe1 rather than
    0x87 in the more obvious mappings.

    The related field multipler can use this mapping but if you want to
    use an alternative (e.g hardware) multiplier that uses a different
    polynomial field representation, you can do so by changing the form
    used for the field elements when this alternative multiplier is used.

    If GF_REPRESENTATION is defined as one of:

        REVERSE_BITS                      // change to LL
        REVERSE_BYTES | REVERSE_BITS      // change to BL
        REVERSE_NONE                      // no change
        REVERSE_BYTES                     // change to BB

    then an appropriate change of representation will occur before and
    after calls to your revised field multiplier. To use this you need
    to add gf_convert.c to your application.
*/

#if defined(__cplusplus)
extern "C" {
#endif

#if 1
#undef GF_REPRESENTATION
#elif 0
#define GF_REPRESENTATION REVERSE_BITS
#elif 0
#define GF_REPRESENTATION REVERSE_BYTES | REVERSE_BITS
#elif 0
#define GF_REPRESENTATION REVERSE_NONE
#elif 0
#define GF_REPRESENTATION REVERSE_BITS
#endif

#define BLOCK_SIZE   GCM_BLOCK_SIZE /* block length                 */
#define BLK_ADR_MASK (BLOCK_SIZE - 1) /* mask for 'in block' address  */
#define CTR_POS      12

#define inc_ctr(x)                                 \
    {                                              \
        int i = BLOCK_SIZE;                        \
        while(i-- > CTR_POS && !++(UI8_PTR(x)[i])) \
            ;                                      \
    }

ret_type gcm_init_and_key(/* initialise mode and set key  */
                          const unsigned char key[], /* the key value                */
                          gcm_ctx ctx[1]) /* the mode context             */
{
    memset(ctx->ghash_h, 0, sizeof(ctx->ghash_h));

    /* set the AES key                          */
    aes_encrypt_key256(key, ctx->aes);

    /* compute E(0) (for the hash function)     */
    aes_encrypt(UI8_PTR(ctx->ghash_h), UI8_PTR(ctx->ghash_h), ctx->aes);

#if defined(GF_REPRESENTATION)
    convert_representation(ctx->ghash_h, ctx->ghash_h, GF_REPRESENTATION);
#endif

#if defined(TABLES_64K)
    init_64k_table(ctx->ghash_h, ctx->gf_t64k);
#elif defined(TABLES_8K)
    init_8k_table(ctx->ghash_h, ctx->gf_t8k);
#elif defined(TABLES_4K)
    init_4k_table(ctx->ghash_h, ctx->gf_t4k);
#elif defined(TABLES_256)
    init_256_table(ctx->ghash_h, ctx->gf_t256);
#endif
#if defined(GF_REPRESENTATION)
    convert_representation(ctx->ghash_h, ctx->ghash_h, GF_REPRESENTATION);
#endif
    return RETURN_GOOD;
}

void gf_mul_hh(gf_t a, gcm_ctx ctx[1]) {
#if defined(GF_REPRESENTATION) || !defined(NO_TABLES)
    gf_t scr = {0};
#endif
#if defined(GF_REPRESENTATION)
    convert_representation(a, a, GF_REPRESENTATION);
#endif

#if defined(TABLES_64K)
    gf_mul_64k(a, ctx->gf_t64k, scr);
#elif defined(TABLES_8K)
    gf_mul_8k(a, ctx->gf_t8k, scr);
#elif defined(TABLES_4K)
    gf_mul_4k(a, ctx->gf_t4k, scr);
#elif defined(TABLES_256)
    gf_mul_256(a, ctx->gf_t256, scr);
#else
#if defined(GF_REPRESENTATION)
    convert_representation(scr, ctx->ghash_h, GF_REPRESENTATION);
    gf_mul(a, scr);
#else
    gf_mul(a, ctx->ghash_h);
#endif
#endif

#if defined(GF_REPRESENTATION)
    convert_representation(a, a, GF_REPRESENTATION);
#endif
}

ret_type gcm_init_message(/* initialise a new message     */
                          const unsigned char iv[], /* the initialisation vector    */
                          unsigned long iv_len, /* and its length in bytes      */
                          gcm_ctx ctx[1]) /* the mode context             */
{
    uint32_t i = 0, n_pos = 0;
    uint8_t* p = NULL;

    memset(ctx->ctr_val, 0, BLOCK_SIZE);
    if(iv_len == CTR_POS) {
        memcpy(ctx->ctr_val, iv, CTR_POS);
        UI8_PTR(ctx->ctr_val)[15] = 0x01;
    } else {
        n_pos = iv_len;
        while(n_pos >= BLOCK_SIZE) {
            xor_block_aligned(ctx->ctr_val, ctx->ctr_val, iv);
            n_pos -= BLOCK_SIZE;
            iv += BLOCK_SIZE;
            gf_mul_hh(ctx->ctr_val, ctx);
        }

        if(n_pos) {
            p = UI8_PTR(ctx->ctr_val);
            while(n_pos-- > 0)
                *p++ ^= *iv++;
            gf_mul_hh(ctx->ctr_val, ctx);
        }
        n_pos = (iv_len << 3);
        for(i = BLOCK_SIZE - 1; n_pos; --i, n_pos >>= 8)
            UI8_PTR(ctx->ctr_val)[i] ^= (unsigned char)n_pos;
        gf_mul_hh(ctx->ctr_val, ctx);
    }

    ctx->y0_val = *UI32_PTR(UI8_PTR(ctx->ctr_val) + CTR_POS);
    memset(ctx->hdr_ghv, 0, BLOCK_SIZE);
    memset(ctx->txt_ghv, 0, BLOCK_SIZE);
    ctx->hdr_cnt  = 0;
    ctx->txt_ccnt = ctx->txt_acnt = 0;
    return RETURN_GOOD;
}

ret_type gcm_auth_header(/* authenticate the header      */
                         const unsigned char hdr[], /* the header buffer            */
                         unsigned long hdr_len, /* and its length in bytes      */
                         gcm_ctx ctx[1]) /* the mode context             */
{
    uint32_t cnt = 0, b_pos = (uint32_t)ctx->hdr_cnt & BLK_ADR_MASK;

    if(!hdr_len) return RETURN_GOOD;

    if(ctx->hdr_cnt && b_pos == 0) gf_mul_hh(ctx->hdr_ghv, ctx);

    if(!((hdr - (UI8_PTR(ctx->hdr_ghv) + b_pos)) & BUF_ADRMASK)) {
        while(cnt < hdr_len && (b_pos & BUF_ADRMASK))
            UI8_PTR(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];

        while(cnt + BUF_INC <= hdr_len && b_pos <= BLOCK_SIZE - BUF_INC) {
            *UNIT_PTR(UI8_PTR(ctx->hdr_ghv) + b_pos) ^= *UNIT_PTR(hdr + cnt);
            cnt += BUF_INC;
            b_pos += BUF_INC;
        }

        while(cnt + BLOCK_SIZE <= hdr_len) {
            gf_mul_hh(ctx->hdr_ghv, ctx);
            xor_block_aligned(ctx->hdr_ghv, ctx->hdr_ghv, hdr + cnt);
            cnt += BLOCK_SIZE;
        }
    } else {
        while(cnt < hdr_len && b_pos < BLOCK_SIZE)
            UI8_PTR(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];

        while(cnt + BLOCK_SIZE <= hdr_len) {
            gf_mul_hh(ctx->hdr_ghv, ctx);
            xor_block(ctx->hdr_ghv, ctx->hdr_ghv, hdr + cnt);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < hdr_len) {
        if(b_pos == BLOCK_SIZE) {
            gf_mul_hh(ctx->hdr_ghv, ctx);
            b_pos = 0;
        }
        UI8_PTR(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];
    }

    ctx->hdr_cnt += cnt;
    return RETURN_GOOD;
}

ret_type gcm_auth_data(/* authenticate ciphertext data */
                       const unsigned char data[], /* the data buffer              */
                       unsigned long data_len, /* and its length in bytes      */
                       gcm_ctx ctx[1]) /* the mode context             */
{
    uint32_t cnt = 0, b_pos = (uint32_t)ctx->txt_acnt & BLK_ADR_MASK;

    if(!data_len) return RETURN_GOOD;

    if(ctx->txt_acnt && b_pos == 0) gf_mul_hh(ctx->txt_ghv, ctx);

    if(!((data - (UI8_PTR(ctx->txt_ghv) + b_pos)) & BUF_ADRMASK)) {
        while(cnt < data_len && (b_pos & BUF_ADRMASK))
            UI8_PTR(ctx->txt_ghv)[b_pos++] ^= data[cnt++];

        while(cnt + BUF_INC <= data_len && b_pos <= BLOCK_SIZE - BUF_INC) {
            *UNIT_PTR(UI8_PTR(ctx->txt_ghv) + b_pos) ^= *UNIT_PTR(data + cnt);
            cnt += BUF_INC;
            b_pos += BUF_INC;
        }

        while(cnt + BLOCK_SIZE <= data_len) {
            gf_mul_hh(ctx->txt_ghv, ctx);
            xor_block_aligned(ctx->txt_ghv, ctx->txt_ghv, data + cnt);
            cnt += BLOCK_SIZE;
        }
    } else {
        while(cnt < data_len && b_pos < BLOCK_SIZE)
            UI8_PTR(ctx->txt_ghv)[b_pos++] ^= data[cnt++];

        while(cnt + BLOCK_SIZE <= data_len) {
            gf_mul_hh(ctx->txt_ghv, ctx);
            xor_block(ctx->txt_ghv, ctx->txt_ghv, data + cnt);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < data_len) {
        if(b_pos == BLOCK_SIZE) {
            gf_mul_hh(ctx->txt_ghv, ctx);
            b_pos = 0;
        }
        UI8_PTR(ctx->txt_ghv)[b_pos++] ^= data[cnt++];
    }

    ctx->txt_acnt += cnt;
    return RETURN_GOOD;
}

ret_type gcm_crypt_data(/* encrypt or decrypt data      */
                        unsigned char data[], /* the data buffer              */
                        unsigned long data_len, /* and its length in bytes      */
                        gcm_ctx ctx[1]) /* the mode context             */
{
    uint32_t cnt = 0, b_pos = (uint32_t)ctx->txt_ccnt & BLK_ADR_MASK;

    if(!data_len) return RETURN_GOOD;

    if(!((data - (UI8_PTR(ctx->enc_ctr) + b_pos)) & BUF_ADRMASK)) {
        if(b_pos) {
            while(cnt < data_len && (b_pos & BUF_ADRMASK))
                data[cnt++] ^= UI8_PTR(ctx->enc_ctr)[b_pos++];

            while(cnt + BUF_INC <= data_len && b_pos <= BLOCK_SIZE - BUF_INC) {
                *UNIT_PTR(data + cnt) ^= *UNIT_PTR(UI8_PTR(ctx->enc_ctr) + b_pos);
                cnt += BUF_INC;
                b_pos += BUF_INC;
            }
        }

        while(cnt + BLOCK_SIZE <= data_len) {
            inc_ctr(ctx->ctr_val);
            aes_encrypt(UI8_PTR(ctx->ctr_val), UI8_PTR(ctx->enc_ctr), ctx->aes);
            xor_block_aligned(data + cnt, data + cnt, ctx->enc_ctr);
            cnt += BLOCK_SIZE;
        }
    } else {
        if(b_pos)
            while(cnt < data_len && b_pos < BLOCK_SIZE)
                data[cnt++] ^= UI8_PTR(ctx->enc_ctr)[b_pos++];

        while(cnt + BLOCK_SIZE <= data_len) {
            inc_ctr(ctx->ctr_val);
            aes_encrypt(UI8_PTR(ctx->ctr_val), UI8_PTR(ctx->enc_ctr), ctx->aes);
            xor_block(data + cnt, data + cnt, ctx->enc_ctr);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < data_len) {
        if(b_pos == BLOCK_SIZE || !b_pos) {
            inc_ctr(ctx->ctr_val);
            aes_encrypt(UI8_PTR(ctx->ctr_val), UI8_PTR(ctx->enc_ctr), ctx->aes);
            b_pos = 0;
        }
        data[cnt++] ^= UI8_PTR(ctx->enc_ctr)[b_pos++];
    }

    ctx->txt_ccnt += cnt;
    return RETURN_GOOD;
}

ret_type gcm_compute_tag(/* compute authentication tag   */
                         unsigned char tag[], /* the buffer for the tag       */
                         unsigned long tag_len, /* and its length in bytes      */
                         gcm_ctx ctx[1]) /* the mode context             */
{
    uint32_t i = 0, ln = 0;
    gf_t tbuf = {0};

    if(ctx->txt_acnt != ctx->txt_ccnt && ctx->txt_ccnt > 0) return RETURN_ERROR;

    gf_mul_hh(ctx->hdr_ghv, ctx);
    gf_mul_hh(ctx->txt_ghv, ctx);

    if(ctx->hdr_cnt) {
        ln = (uint32_t)((ctx->txt_acnt + BLOCK_SIZE - 1) / BLOCK_SIZE);
        if(ln) {
#if 1 /* alternative versions of the exponentiation operation */
            memcpy(tbuf, ctx->ghash_h, BLOCK_SIZE);
#if defined(GF_REPRESENTATION)
            convert_representation(tbuf, tbuf, GF_REPRESENTATION);
            convert_representation(ctx->hdr_ghv, ctx->hdr_ghv, GF_REPRESENTATION);
#endif
            for(;;) {
                if(ln & 1) {
                    gf_mul((void*)ctx->hdr_ghv, tbuf);
                }
                if(!(ln >>= 1)) break;
                gf_mul(tbuf, tbuf);
            }
#else /* this one seems slower on x86 and x86_64 :-( */
            i = ln | ln >> 1;
            i |= i >> 2;
            i |= i >> 4;
            i |= i >> 8;
            i |= i >> 16;
            i &= ~(i >> 1);
            memset(tbuf, 0, BLOCK_SIZE);
            UI8_PTR(tbuf)[0] = 0x80;
            while(i) {
#if defined(GF_REPRESENTATION)
                convert_representation(tbuf, tbuf, GF_REPRESENTATION);
#endif
                gf_mul(tbuf, tbuf);
#if defined(GF_REPRESENTATION)
                convert_representation(tbuf, tbuf, GF_REPRESENTATION);
#endif
                if(i & ln) gf_mul_hh(tbuf, ctx);
                i >>= 1;
            }
#if defined(GF_REPRESENTATION)
            convert_representation(tbuf, tbuf, GF_REPRESENTATION);
            convert_representation(ctx->hdr_ghv, ctx->hdr_ghv, GF_REPRESENTATION);
#endif
            gf_mul((void*)ctx->hdr_ghv, tbuf);
#endif
#if defined(GF_REPRESENTATION)
            convert_representation(ctx->hdr_ghv, ctx->hdr_ghv, GF_REPRESENTATION);
#endif
        }
    }

    i = BLOCK_SIZE;
#ifdef BRG_UI64
    {
        uint64_t tm = ((uint64_t)ctx->txt_acnt) << 3;
        while(i-- > 0) {
            UI8_PTR(ctx->hdr_ghv)[i] ^= UI8_PTR(ctx->txt_ghv)[i] ^ (unsigned char)tm;
            tm = (i == 8 ? (((uint64_t)ctx->hdr_cnt) << 3) : tm >> 8);
        }
    }
#else
    {
        uint32_t tm = ctx->txt_acnt << 3;

        while(i-- > 0) {
            UI8_PTR(ctx->hdr_ghv)[i] ^= UI8_PTR(ctx->txt_ghv)[i] ^ (unsigned char)tm;
            if(i & 3)
                tm >>= 8;
            else if(i == 4)
                tm = ctx->txt_acnt >> 29;
            else if(i == 8)
                tm = ctx->hdr_cnt << 3;
            else
                tm = ctx->hdr_cnt >> 29;
        }
    }
#endif

    gf_mul_hh(ctx->hdr_ghv, ctx);

    memcpy(ctx->enc_ctr, ctx->ctr_val, BLOCK_SIZE);
    *UI32_PTR(UI8_PTR(ctx->enc_ctr) + CTR_POS) = ctx->y0_val;
    aes_encrypt(UI8_PTR(ctx->enc_ctr), UI8_PTR(ctx->enc_ctr), ctx->aes);
    for(i = 0; i < (unsigned int)tag_len; ++i)
        tag[i] = (unsigned char)(UI8_PTR(ctx->hdr_ghv)[i] ^ UI8_PTR(ctx->enc_ctr)[i]);

    return (ctx->txt_ccnt == ctx->txt_acnt ? RETURN_GOOD : RETURN_WARN);
}

ret_type gcm_end(/* clean up and end operation   */
                 gcm_ctx ctx[1]) /* the mode context             */
{
    memset(ctx, 0, sizeof(gcm_ctx));
    return RETURN_GOOD;
}

ret_type gcm_encrypt(/* encrypt & authenticate data  */
                     unsigned char data[], /* the data buffer              */
                     unsigned long data_len, /* and its length in bytes      */
                     gcm_ctx ctx[1]) /* the mode context             */
{
    gcm_crypt_data(data, data_len, ctx);
    gcm_auth_data(data, data_len, ctx);
    return RETURN_GOOD;
}

ret_type gcm_decrypt(/* authenticate & decrypt data  */
                     unsigned char data[], /* the data buffer              */
                     unsigned long data_len, /* and its length in bytes      */
                     gcm_ctx ctx[1]) /* the mode context             */
{
    gcm_auth_data(data, data_len, ctx);
    gcm_crypt_data(data, data_len, ctx);
    return RETURN_GOOD;
}

ret_type gcm_encrypt_message(/* encrypt an entire message    */
                             const unsigned char iv[], /* the initialisation vector    */
                             unsigned long iv_len, /* and its length in bytes      */
                             const unsigned char hdr[], /* the header buffer            */
                             unsigned long hdr_len, /* and its length in bytes      */
                             unsigned char msg[], /* the message buffer           */
                             unsigned long msg_len, /* and its length in bytes      */
                             unsigned char tag[], /* the buffer for the tag       */
                             unsigned long tag_len, /* and its length in bytes      */
                             gcm_ctx ctx[1]) /* the mode context             */
{
    gcm_init_message(iv, iv_len, ctx);
    gcm_auth_header(hdr, hdr_len, ctx);
    gcm_encrypt(msg, msg_len, ctx);
    return gcm_compute_tag(tag, tag_len, ctx) ? RETURN_ERROR : RETURN_GOOD;
}

ret_type gcm_decrypt_message(/* decrypt an entire message    */
                             const unsigned char iv[], /* the initialisation vector    */
                             unsigned long iv_len, /* and its length in bytes      */
                             const unsigned char hdr[], /* the header buffer            */
                             unsigned long hdr_len, /* and its length in bytes      */
                             unsigned char msg[], /* the message buffer           */
                             unsigned long msg_len, /* and its length in bytes      */
                             const unsigned char tag[], /* the buffer for the tag       */
                             unsigned long tag_len, /* and its length in bytes      */
                             gcm_ctx ctx[1]) /* the mode context             */
{
    uint8_t local_tag[BLOCK_SIZE] = {0};
    ret_type rr                   = 0;

    gcm_init_message(iv, iv_len, ctx);
    gcm_auth_header(hdr, hdr_len, ctx);
    gcm_decrypt(msg, msg_len, ctx);
    rr = gcm_compute_tag(local_tag, tag_len, ctx);
    return (rr != RETURN_GOOD || memcmp(tag, local_tag, tag_len)) ? RETURN_ERROR : RETURN_GOOD;
}

#if defined(__cplusplus)
}
#endif
