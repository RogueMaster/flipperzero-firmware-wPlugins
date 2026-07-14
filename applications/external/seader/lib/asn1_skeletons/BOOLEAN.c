/*-
 * Copyright (c) 2003, 2005 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <asn_codecs_prim.h>
#include <BOOLEAN.h>

/*
 * BOOLEAN basic type description.
 */
static const ber_tlv_tag_t asn_DEF_BOOLEAN_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (1 << 2))
};
asn_TYPE_operation_t asn_OP_BOOLEAN = {
	BOOLEAN_free,
	BOOLEAN_print,
	BOOLEAN_compare,
	BOOLEAN_decode_ber,
	BOOLEAN_encode_der,
#ifndef ASN_DISABLE_XER_SUPPORT
	BOOLEAN_decode_xer,
	BOOLEAN_encode_xer,
#else
	0, 0,
#endif  /* ASN_DISABLE_XER_SUPPORT */
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	BOOLEAN_decode_oer,
	BOOLEAN_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	BOOLEAN_decode_uper,	/* Unaligned PER decoder */
	BOOLEAN_encode_uper,	/* Unaligned PER encoder */
#endif	/* ASN_DISABLE_PER_SUPPORT */
#ifdef  ASN_DISABLE_RANDOM_FILL
	0,
#else
	BOOLEAN_random_fill,
#endif  /* ASN_DISABLE_RANDOM_FILL */
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_BOOLEAN = {
	"BOOLEAN",
	"BOOLEAN",
	&asn_OP_BOOLEAN,
	asn_DEF_BOOLEAN_tags,
	sizeof(asn_DEF_BOOLEAN_tags) / sizeof(asn_DEF_BOOLEAN_tags[0]),
	asn_DEF_BOOLEAN_tags,	/* Same as above */
	sizeof(asn_DEF_BOOLEAN_tags) / sizeof(asn_DEF_BOOLEAN_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * Decode BOOLEAN type.
 */
asn_dec_rval_t
BOOLEAN_decode_ber(const asn_codec_ctx_t *opt_codec_ctx,
                   const asn_TYPE_descriptor_t *td, void **bool_value,
                   const void *buf_ptr, size_t size, int tag_mode) {
    BOOLEAN_t *st = (BOOLEAN_t *)*bool_value;
	asn_dec_rval_t rval;
	ber_tlv_len_t length;
	ber_tlv_len_t lidx;

	if(st == NULL) {
		st = (BOOLEAN_t *)(*bool_value = CALLOC(1, sizeof(*st)));
		if(st == NULL) {
			rval.code = RC_FAIL;
			rval.consumed = 0;
			return rval;
		}
	}

	ASN_DEBUG("Decoding %s as BOOLEAN (tm=%d)",
		td->name, tag_mode);

	/*
	 * Check tags.
	 */
	rval = ber_check_tags(opt_codec_ctx, td, 0, buf_ptr, size,
		tag_mode, 0, &length, 0);
	if(rval.code != RC_OK)
		return rval;

	ASN_DEBUG("Boolean length is %d bytes", (int)length);

	buf_ptr = ((const char *)buf_ptr) + rval.consumed;
	size -= rval.consumed;
	if(length > (ber_tlv_len_t)size) {
		rval.code = RC_WMORE;
		rval.consumed = 0;
		return rval;
	}

	/*
	 * Compute boolean value.
	 */
	for(*st = 0, lidx = 0;
		(lidx < length) && *st == 0; lidx++) {
		/*
		 * Very simple approach: read bytes until the end or
		 * value is already TRUE.
		 * BOOLEAN is not supposed to contain meaningful data anyway.
		 */
		*st |= ((const uint8_t *)buf_ptr)[lidx];
	}

	rval.code = RC_OK;
	rval.consumed += length;

	ASN_DEBUG("Took %ld/%ld bytes to encode %s, value=%d",
		(long)rval.consumed, (long)length,
		td->name, *st);
	
	return rval;
}

asn_enc_rval_t
BOOLEAN_encode_der(const asn_TYPE_descriptor_t *td, const void *sptr,
                   int tag_mode, ber_tlv_tag_t tag, asn_app_consume_bytes_f *cb,
                   void *app_key) {
    asn_enc_rval_t erval;
    const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;

    erval.encoded = der_write_tags(td, 1, tag_mode, 0, tag, cb, app_key);
	if(erval.encoded == -1) {
		erval.failed_type = td;
		erval.structure_ptr = sptr;
		return erval;
	}

	if(cb) {
		uint8_t bool_value;

		bool_value = *st ? 0xff : 0; /* 0xff mandated by DER */

		if(cb(&bool_value, 1, app_key) < 0) {
			erval.encoded = -1;
			erval.failed_type = td;
			erval.structure_ptr = sptr;
			return erval;
		}
	}

	erval.encoded += 1;

	ASN__ENCODED_OK(erval);
}


#ifndef ASN_DISABLE_XER_SUPPORT
/*
 * Decode the chunk of XML text encoding INTEGER.
 */
static enum xer_pbd_rval
BOOLEAN__xer_body_decode(const asn_TYPE_descriptor_t *td, void *sptr,
                         const void *chunk_buf, size_t chunk_size) {
    BOOLEAN_t *st = (BOOLEAN_t *)sptr;
	const char *p = (const char *)chunk_buf;

	(void)td;

	if(chunk_size && p[0] == 0x3c /* '<' */) {
		switch(xer_check_tag(chunk_buf, chunk_size, "false")) {
		case XCT_BOTH:
			/* "<false/>" */
			*st = 0;
			break;
		case XCT_UNKNOWN_BO:
			if(xer_check_tag(chunk_buf, chunk_size, "true")
					!= XCT_BOTH)
				return XPBD_BROKEN_ENCODING;
			/* "<true/>" */
			*st = 1;	/* Or 0xff as in DER?.. */
			break;
		default:
			return XPBD_BROKEN_ENCODING;
		}
		return XPBD_BODY_CONSUMED;
	} else {
		return XPBD_BROKEN_ENCODING;
	}
}


asn_dec_rval_t
BOOLEAN_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                   const asn_TYPE_descriptor_t *td, void **sptr,
                   const char *opt_mname, const void *buf_ptr, size_t size) {
    return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(BOOLEAN_t), opt_mname, buf_ptr, size,
		BOOLEAN__xer_body_decode);
}

asn_enc_rval_t
BOOLEAN_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	asn_enc_rval_t er = {0, 0, 0};

	(void)ilevel;
	(void)flags;

	if(!st) ASN__ENCODE_FAILED;

	if(*st) {
		ASN__CALLBACK("<true/>", 7);
	} else {
		ASN__CALLBACK("<false/>", 8);
	}

	ASN__ENCODED_OK(er);
cb_failed:
	ASN__ENCODE_FAILED;
}
#endif /* ASN_DISABLE_XER_SUPPORT */

int
BOOLEAN_print(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
              asn_app_consume_bytes_f *cb, void *app_key) {
    const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	const char *buf;
	size_t buflen;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(st) {
		if(*st) {
			buf = "TRUE";
			buflen = 4;
		} else {
			buf = "FALSE";
			buflen = 5;
		}
	} else {
		buf = "<absent>";
		buflen = 8;
	}

	return (cb(buf, buflen, app_key) < 0) ? -1 : 0;
}

void
BOOLEAN_free(const asn_TYPE_descriptor_t *td, void *ptr,
             enum asn_struct_free_method method) {
    if(td && ptr) {
        switch(method) {
        case ASFM_FREE_EVERYTHING:
            FREEMEM(ptr);
            break;
        case ASFM_FREE_UNDERLYING:
            break;
        case ASFM_FREE_UNDERLYING_AND_RESET:
            memset(ptr, 0, sizeof(BOOLEAN_t));
            break;
        }
    }
}

#ifndef ASN_DISABLE_PER_SUPPORT

asn_dec_rval_t
BOOLEAN_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                    const asn_TYPE_descriptor_t *td,
                    const asn_per_constraints_t *constraints, void **sptr,
                    asn_per_data_t *pd) {
    asn_dec_rval_t rv;
	BOOLEAN_t *st = (BOOLEAN_t *)*sptr;

	(void)opt_codec_ctx;
    (void)td;
	(void)constraints;

	if(!st) {
		st = (BOOLEAN_t *)(*sptr = MALLOC(sizeof(*st)));
		if(!st) ASN__DECODE_FAILED;
	}

	/*
	 * Extract a single bit
	 */
	switch(per_get_few_bits(pd, 1)) {
	case 1: *st = 1; break;
	case 0: *st = 0; break;
	case -1: default: ASN__DECODE_STARVED;
	}

	ASN_DEBUG("%s decoded as %s", td->name, *st ? "TRUE" : "FALSE");

	rv.code = RC_OK;
	rv.consumed = 1;
	return rv;
}


asn_enc_rval_t
BOOLEAN_encode_uper(const asn_TYPE_descriptor_t *td,
                    const asn_per_constraints_t *constraints, const void *sptr,
                    asn_per_outp_t *po) {
    const BOOLEAN_t *st = (const BOOLEAN_t *)sptr;
	asn_enc_rval_t er = { 0, 0, 0 };

	(void)constraints;

	if(!st) ASN__ENCODE_FAILED;

	if(per_put_few_bits(po, *st ? 1 : 0, 1))
		ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

#endif /* ASN_DISABLE_PER_SUPPORT */

#ifndef  ASN_DISABLE_OER_SUPPORT

/*
 * Encode as Canonical OER.
 */
asn_enc_rval_t
BOOLEAN_encode_oer(const asn_TYPE_descriptor_t *td,
                   const asn_oer_constraints_t *constraints, const void *sptr,
                   asn_app_consume_bytes_f *cb, void *app_key) {
    asn_enc_rval_t er = { 1, 0, 0 };
    const BOOLEAN_t *st = sptr;
    uint8_t bool_value = *st ? 0xff : 0; /* 0xff mandated by OER */

    (void)td;
    (void)constraints;  /* Constraints are unused in OER */

    if(cb(&bool_value, 1, app_key) < 0) {
        ASN__ENCODE_FAILED;
    } else {
        ASN__ENCODED_OK(er);
    }
}

asn_dec_rval_t
BOOLEAN_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                   const asn_TYPE_descriptor_t *td,
                   const asn_oer_constraints_t *constraints, void **sptr,
                   const void *ptr, size_t size) {
    asn_dec_rval_t ok = {RC_OK, 1};
    BOOLEAN_t *st;

    (void)opt_codec_ctx;
    (void)td;
    (void)constraints; /* Constraints are unused in OER */

    if(size < 1) {
        ASN__DECODE_STARVED;
    }

    if(!(st = *sptr)) {
        st = (BOOLEAN_t *)(*sptr = CALLOC(1, sizeof(*st)));
        if(!st) ASN__DECODE_FAILED;
    }

    *st = *(const uint8_t *)ptr;

    return ok;
}



#endif

int
BOOLEAN_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
                const void *bptr) {
    const BOOLEAN_t *a = aptr;
    const BOOLEAN_t *b = bptr;

    (void)td;

    if(a && b) {
        if(!*a == !*b) {    /* TRUE can be encoded by any non-zero byte. */
            return 0;
        } else if(!*a) {
            return -1;
        } else {
            return 1;
        }
    } else if(!a) {
        return -1;
    } else {
        return 1;
    }
}

#ifndef ASN_DISABLE_RANDOM_FILL
asn_random_fill_result_t
BOOLEAN_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                    const asn_encoding_constraints_t *constraints,
                    size_t max_length) {
    asn_random_fill_result_t result_ok = {ARFILL_OK, 1};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    BOOLEAN_t *st = *sptr;

    if(max_length == 0) return result_skipped;

    if(st == NULL) {
        st = (BOOLEAN_t *)(*sptr = CALLOC(1, sizeof(*st)));
        if(st == NULL) {
            return result_failed;
        }
    }

    if(!constraints || !constraints->per_constraints)
        constraints = &td->encoding_constraints;
    if(constraints->per_constraints) {
        const asn_per_constraint_t *pc = &constraints->per_constraints->value;
        if(pc->flags & APC_CONSTRAINED) {
            *st = asn_random_between(pc->lower_bound, pc->upper_bound);
            return result_ok;
        }
    }

    /* Simulate booleans that are sloppily set and biased. */
    switch(asn_random_between(0, 7)) {
    case 0:
    case 1:
    case 2:
        *st = 0; break;
    case 3: *st = -1; break;
    case 4: *st = 1; break;
    case 5: *st = INT_MIN; break;
    case 6: *st = INT_MAX; break;
    default:
        *st = asn_random_between(INT_MIN, INT_MAX);
        break;
    }
    return result_ok;
}
#endif /* ASN_DISABLE_RANDOM_FILL */
