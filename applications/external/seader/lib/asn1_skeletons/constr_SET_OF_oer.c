/*
 * Copyright (c) 2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef ASN_DISABLE_OER_SUPPORT

#include <asn_internal.h>
#include <constr_SET_OF.h>
#include <asn_SET_OF.h>
#include <errno.h>

/*
 * This macro "eats" the part of the buffer which is definitely "consumed",
 * i.e. was correctly converted into local representation or rightfully skipped.
 */
#undef  ADVANCE
#define ADVANCE(num_bytes)                   \
    do {                                     \
        size_t num = num_bytes;              \
        ptr = ((const char *)ptr) + num;     \
        size -= num;                         \
        consumed_myself += num;              \
    } while(0)

/*
 * Switch to the next phase of parsing.
 */
#undef  NEXT_PHASE
#define NEXT_PHASE(ctx) \
    do {                \
        ctx->phase++;   \
        ctx->step = 0;  \
    } while(0)
#undef  SET_PHASE
#define SET_PHASE(ctx, value) \
    do {                      \
        ctx->phase = value;   \
        ctx->step = 0;        \
    } while(0)

/*
 * Return a standardized complex structure.
 */
#undef  RETURN
#define RETURN(_code)                    \
    do {                                 \
        asn_dec_rval_t rval;             \
        rval.code = _code;               \
        rval.consumed = consumed_myself; \
        return rval;                     \
    } while(0)

/*
 * The SEQUENCE OF and SET OF values utilize a "quantity field".
 * It is is a pointless combination of #8.6 (length determinant, capable
 * of encoding tiny and huge numbers in the shortest possible number of octets)
 * and the variable sized integer. What could have been encoded by #8.6 alone
 * is required to be encoded by #8.6 followed by that number of unsigned octets.
 * This doesn't make too much sense. It seems that the original version of OER
 * standard have been using the unconstrained unsigned integer as a quantity
 * field, and this legacy have gone through ISO/ITU-T standardization process.
 */
static ssize_t
oer_fetch_quantity(const void *ptr, size_t size, size_t *qty_r) {
    const uint8_t *b;
    const uint8_t *bend;
    size_t len = 0;
    size_t qty;

    ssize_t len_len = oer_fetch_length(ptr, size, &len);
    if(len_len <= 0) {
        *qty_r = 0;
        return len_len;
    }

    if((len_len + len) > size) {
        *qty_r = 0;
        return 0;
    }

    b = (const uint8_t *)ptr + len_len;
    bend = b + len;

    /* Skip the leading 0-bytes */
    for(; b < bend && *b == 0; b++) {
    }

    if((bend - b) > (ssize_t)sizeof(size_t)) {
        /* Length is not representable by the native size_t type */
        *qty_r = 0;
        return -1;
    }

    for(qty = 0; b < bend; b++) {
        qty = (qty << 8) + *b;
    }

    if(qty > RSIZE_MAX) { /* A bit of C11 validation */
        *qty_r = 0;
        return -1;
    }

    *qty_r = qty;
    assert((size_t)len_len + len == (size_t)(bend - (const uint8_t *)ptr));
    return len_len + len;
}

asn_dec_rval_t
SET_OF_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                  const asn_TYPE_descriptor_t *td,
                  const asn_oer_constraints_t *constraints, void **struct_ptr,
                  const void *ptr, size_t size) {
    const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    asn_dec_rval_t rval = {RC_OK, 0};
    void *st = *struct_ptr; /* Target structure */
    asn_struct_ctx_t *ctx; /* Decoder context */
    size_t consumed_myself = 0; /* Consumed bytes from ptr. */

    (void)constraints;

    if(ASN__STACK_OVERFLOW_CHECK(opt_codec_ctx))
        ASN__DECODE_FAILED;

    /*
     * Create the target structure if it is not present already.
     */
    if(st == 0) {
        st = *struct_ptr = CALLOC(1, specs->struct_size);
        if(st == 0) {
            RETURN(RC_FAIL);
        }
    }

    /*
     * Restore parsing context.
     */
    ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);

    /*
     * Start to parse where left previously.
     */
    switch(ctx->phase) {
    case 0: {
        /*
         * Fetch number of elements to decode.
         */
        size_t length = 0;
        size_t len_size = oer_fetch_quantity(ptr, size, &length);
        switch(len_size) {
        case 0:
            RETURN(RC_WMORE);
        case -1:
            RETURN(RC_FAIL);
        default:
            ADVANCE(len_size);
            ctx->left = length;
        }
    }
        NEXT_PHASE(ctx);
        /* FALL THROUGH */
    case 1: {
        /* Decode components of the extension root */
        asn_TYPE_member_t *elm = td->elements;
        asn_anonymous_set_ *list = _A_SET_FROM_VOID(st);
        const void *base_ptr = ptr;
        ber_tlv_len_t base_ctx_left = ctx->left;

        assert(td->elements_count == 1);

        ASN_DEBUG("OER SET OF %s Decoding PHASE 1", td->name);

        for(; ctx->left > 0; ctx->left--) {
            asn_dec_rval_t rv = elm->type->op->oer_decoder(
                opt_codec_ctx, elm->type,
                elm->encoding_constraints.oer_constraints, &ctx->ptr, ptr,
                size);
            ADVANCE(rv.consumed);
            switch(rv.code) {
            case RC_OK:
                if(ASN_SET_ADD(list, ctx->ptr) != 0) {
                    RETURN(RC_FAIL);
                } else {
                    ctx->ptr = 0;
                    /*
                     * This check is to avoid compression bomb with
                     * specs like SEQUENCE/SET OF NULL which don't
                     * consume data at all.
                     */
                    if(rv.consumed == 0 && base_ptr == ptr
                       && (base_ctx_left - ctx->left) > 200) {
                        ASN__DECODE_FAILED;
                    }
                    break;
                }
            case RC_WMORE:
                RETURN(RC_WMORE);
            case RC_FAIL:
                ASN_STRUCT_FREE(*elm->type, ctx->ptr);
                ctx->ptr = 0;
                SET_PHASE(ctx, 3);
                RETURN(RC_FAIL);
            }
        }
        /* Decoded decently. */
        NEXT_PHASE(ctx);
    }
        /* Fall through */
    case 2:
        /* Ignore fully decoded */
        assert(ctx->left == 0);
        RETURN(RC_OK);
    case 3:
        /* Failed to decode. */
        RETURN(RC_FAIL);
    }

    return rval;
}

static ssize_t
oer_put_quantity(size_t qty, asn_app_consume_bytes_f *cb, void *app_key) {
    uint8_t buf[1 + sizeof(size_t)];
    uint8_t *b = &buf[sizeof(size_t)]; /* Last addressable */
    size_t encoded;

    do {
        *b-- = qty;
        qty >>= 8;
    } while(qty);

    *b = sizeof(buf) - (b-buf) - 1;
    encoded = sizeof(buf) - (b-buf);
    if(cb(b, encoded, app_key) < 0)
        return -1;
    return encoded;
}

/*
 * Encode as Canonical OER.
 */
asn_enc_rval_t
SET_OF_encode_oer(const asn_TYPE_descriptor_t *td,
                  const asn_oer_constraints_t *constraints, const void *sptr,
                  asn_app_consume_bytes_f *cb, void *app_key) {
    const asn_TYPE_member_t *elm;
    const asn_anonymous_set_ *list;
    size_t computed_size = 0;
    ssize_t qty_len;
    int n;

    (void)constraints;

    if(!sptr) ASN__ENCODE_FAILED;

    elm = td->elements;
    list = _A_CSET_FROM_VOID(sptr);

    qty_len = oer_put_quantity(list->count, cb, app_key);
    if(qty_len < 0) {
        ASN__ENCODE_FAILED;
    }
    computed_size += qty_len;

    for(n = 0; n < list->count; n++) {
        void *memb_ptr = list->array[n];
        asn_enc_rval_t er;
        er = elm->type->op->oer_encoder(
            elm->type, elm->encoding_constraints.oer_constraints, memb_ptr, cb,
            app_key);
        if(er.encoded < 0) {
            return er;
        } else {
            computed_size += er.encoded;
        }
    }

    {
        asn_enc_rval_t erval;
        erval.encoded = computed_size;
        ASN__ENCODED_OK(erval);
    }
}

#endif  /* ASN_DISABLE_OER_SUPPORT */
