/*-
 * Copyright (c) 2003, 2005, 2006 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <ENUMERATED.h>
#include <NativeEnumerated.h>
#include <asn_codecs_prim.h>	/* Encoder and decoder of a primitive type */

/*
 * ENUMERATED basic type description.
 */
static const ber_tlv_tag_t asn_DEF_ENUMERATED_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (10 << 2))
};
asn_TYPE_operation_t asn_OP_ENUMERATED = {
	ASN__PRIMITIVE_TYPE_free,
	INTEGER_print,			/* Implemented in terms of INTEGER */
	INTEGER_compare,		/* Implemented in terms of INTEGER */
	ber_decode_primitive,
	INTEGER_encode_der,		/* Implemented in terms of INTEGER */
	INTEGER_decode_xer,	/* This is temporary! */
	INTEGER_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	ENUMERATED_decode_oer,
	ENUMERATED_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	ENUMERATED_decode_uper,	/* Unaligned PER decoder */
	ENUMERATED_encode_uper,	/* Unaligned PER encoder */
#endif	/* ASN_DISABLE_PER_SUPPORT */
	ENUMERATED_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_ENUMERATED = {
	"ENUMERATED",
	"ENUMERATED",
	&asn_OP_ENUMERATED,
	asn_DEF_ENUMERATED_tags,
	sizeof(asn_DEF_ENUMERATED_tags) / sizeof(asn_DEF_ENUMERATED_tags[0]),
	asn_DEF_ENUMERATED_tags,	/* Same as above */
	sizeof(asn_DEF_ENUMERATED_tags) / sizeof(asn_DEF_ENUMERATED_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};


#ifndef  ASN_DISABLE_OER_SUPPORT

asn_dec_rval_t
ENUMERATED_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                      const asn_TYPE_descriptor_t *td,
                      const asn_oer_constraints_t *constraints, void **sptr,
                      const void *ptr, size_t size) {
    asn_dec_rval_t rval;
    ENUMERATED_t *st = (ENUMERATED_t *)*sptr;
    long value;
    void *vptr = &value;

    if(!st) {
        st = (ENUMERATED_t *)(*sptr = CALLOC(1, sizeof(*st)));
		if(!st) ASN__DECODE_FAILED;
	}

    rval = NativeEnumerated_decode_oer(opt_codec_ctx, td, constraints,
                                       (void **)&vptr, ptr, size);
    if(rval.code == RC_OK) {
        if(asn_long2INTEGER(st, value)) {
            rval.code = RC_FAIL;
        }
    }
    return rval;
}

asn_enc_rval_t
ENUMERATED_encode_oer(const asn_TYPE_descriptor_t *td,
                      const asn_oer_constraints_t *constraints,
                      const void *sptr, asn_app_consume_bytes_f *cb,
                      void *app_key) {
    const ENUMERATED_t *st = sptr;
	long value;

	if(asn_INTEGER2long(st, &value)) {
        ASN__ENCODE_FAILED;
    }

    return NativeEnumerated_encode_oer(td, constraints, &value, cb, app_key);
}

#endif  /* ASN_DISABLE_OER_SUPPORT */

#ifndef  ASN_DISABLE_PER_SUPPORT

asn_dec_rval_t
ENUMERATED_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                       const asn_TYPE_descriptor_t *td,
                       const asn_per_constraints_t *constraints, void **sptr,
                       asn_per_data_t *pd) {
    asn_dec_rval_t rval;
    ENUMERATED_t *st = (ENUMERATED_t *)*sptr;
    long value;
    void *vptr = &value;

    if(!st) {
        st = (ENUMERATED_t *)(*sptr = CALLOC(1, sizeof(*st)));
        if(!st) ASN__DECODE_FAILED;
	}

    rval = NativeEnumerated_decode_uper(opt_codec_ctx, td, constraints,
                                        (void **)&vptr, pd);
    if(rval.code == RC_OK) {
        if(asn_long2INTEGER(st, value)) {
            rval.code = RC_FAIL;
        }
    }
    return rval;
}

asn_enc_rval_t
ENUMERATED_encode_uper(const asn_TYPE_descriptor_t *td,
                       const asn_per_constraints_t *constraints,
                       const void *sptr, asn_per_outp_t *po) {
    const ENUMERATED_t *st = (const ENUMERATED_t *)sptr;
    long value;

    if(asn_INTEGER2long(st, &value)) {
        ASN__ENCODE_FAILED;
    }

    return NativeEnumerated_encode_uper(td, constraints, &value, po);
}

#endif  /* ASN_DISABLE_PER_SUPPORT */
