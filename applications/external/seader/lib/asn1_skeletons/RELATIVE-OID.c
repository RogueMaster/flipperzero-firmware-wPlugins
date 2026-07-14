/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>.
 * 	All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <RELATIVE-OID.h>
#include <OCTET_STRING.h>
#include <asn_codecs_prim.h>	/* Encoder and decoder of a primitive type */
#include <limits.h>	/* for CHAR_BIT */
#include <errno.h>

/*
 * RELATIVE-OID basic type description.
 */
static const ber_tlv_tag_t asn_DEF_RELATIVE_OID_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (13 << 2))
};
asn_TYPE_operation_t asn_OP_RELATIVE_OID = {
	ASN__PRIMITIVE_TYPE_free,
	RELATIVE_OID_print,
	OCTET_STRING_compare,   /* Implemented in terms of opaque comparison */
	ber_decode_primitive,
	der_encode_primitive,
	RELATIVE_OID_decode_xer,
	RELATIVE_OID_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	RELATIVE_OID_decode_oer,
	RELATIVE_OID_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_uper,
	OCTET_STRING_encode_uper,
#endif	/* ASN_DISABLE_PER_SUPPORT */
	RELATIVE_OID_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_RELATIVE_OID = {
	"RELATIVE-OID",
	"RELATIVE_OID",
	&asn_OP_RELATIVE_OID,
	asn_DEF_RELATIVE_OID_tags,
	sizeof(asn_DEF_RELATIVE_OID_tags)
	    / sizeof(asn_DEF_RELATIVE_OID_tags[0]),
	asn_DEF_RELATIVE_OID_tags,	/* Same as above */
	sizeof(asn_DEF_RELATIVE_OID_tags)
	    / sizeof(asn_DEF_RELATIVE_OID_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

static ssize_t
RELATIVE_OID__dump_body(const RELATIVE_OID_t *st, asn_app_consume_bytes_f *cb, void *app_key) {
    char scratch[32];
    size_t produced = 0;
    size_t off = 0;

    for(;;) {
        asn_oid_arc_t arc;
        ssize_t rd = OBJECT_IDENTIFIER_get_single_arc(st->buf + off,
                                                      st->size - off, &arc);
        if(rd < 0) {
            return -1;
        } else if(rd == 0) {
            /* No more arcs. */
            break;
        } else {
            int ret = snprintf(scratch, sizeof(scratch), "%s%" PRIu32,
                               off ? "." : "", arc);
            if(ret >= (ssize_t)sizeof(scratch)) {
                return -1;
            }
            produced += ret;
            off += rd;
            assert(off <= st->size);
            if(cb(scratch, ret, app_key) < 0) return -1;
        }
    }

    if(off != st->size) {
        ASN_DEBUG("Could not scan to the end of Object Identifier");
        return -1;
    }

	return produced;
}

int
RELATIVE_OID_print(const asn_TYPE_descriptor_t *td, const void *sptr,
                   int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
    const RELATIVE_OID_t *st = (const RELATIVE_OID_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(!st || !st->buf)
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	/* Dump preamble */
	if(cb("{ ", 2, app_key) < 0)
		return -1;

	if(RELATIVE_OID__dump_body(st, cb, app_key) < 0)
		return -1;

	return (cb(" }", 2, app_key) < 0) ? -1 : 0;
}

static enum xer_pbd_rval
RELATIVE_OID__xer_body_decode(const asn_TYPE_descriptor_t *td, void *sptr,
                              const void *chunk_buf, size_t chunk_size) {
    RELATIVE_OID_t *st = (RELATIVE_OID_t *)sptr;
	const char *chunk_end = (const char *)chunk_buf + chunk_size;
	const char *endptr;
	asn_oid_arc_t s_arcs[6];
	asn_oid_arc_t *arcs = s_arcs;
	ssize_t num_arcs;
	int ret;

	(void)td;

    num_arcs = OBJECT_IDENTIFIER_parse_arcs(
        (const char *)chunk_buf, chunk_size, arcs,
        sizeof(s_arcs) / sizeof(s_arcs[0]), &endptr);
    if(num_arcs < 0) {
        /* Expecting at least one arc arcs */
        return XPBD_BROKEN_ENCODING;
    } else if(num_arcs == 0) {
        return XPBD_NOT_BODY_IGNORE;
    }
    assert(endptr == chunk_end);

    if((size_t)num_arcs > sizeof(s_arcs) / sizeof(s_arcs[0])) {
        arcs = (asn_oid_arc_t *)MALLOC(num_arcs * sizeof(arcs[0]));
        if(!arcs) return XPBD_SYSTEM_FAILURE;
        ret = OBJECT_IDENTIFIER_parse_arcs((const char *)chunk_buf, chunk_size,
                                           arcs, num_arcs, &endptr);
        if(ret != num_arcs) {
            return XPBD_SYSTEM_FAILURE; /* assert?.. */
        }
    }

    /*
     * Convert arcs into BER representation.
     */
    ret = RELATIVE_OID_set_arcs(st, arcs, num_arcs);
    if(arcs != s_arcs) FREEMEM(arcs);

    return ret ? XPBD_SYSTEM_FAILURE : XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
RELATIVE_OID_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                        const asn_TYPE_descriptor_t *td, void **sptr,
                        const char *opt_mname, const void *buf_ptr,
                        size_t size) {
    return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(RELATIVE_OID_t), opt_mname,
			buf_ptr, size, RELATIVE_OID__xer_body_decode);
}

asn_enc_rval_t
RELATIVE_OID_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
                        int ilevel, enum xer_encoder_flags_e flags,
                        asn_app_consume_bytes_f *cb, void *app_key) {
    const RELATIVE_OID_t *st = (const RELATIVE_OID_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;	/* Unused argument */
	(void)flags;	/* Unused argument */

	if(!st || !st->buf)
		ASN__ENCODE_FAILED;

	er.encoded = RELATIVE_OID__dump_body(st, cb, app_key);
	if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

ssize_t
RELATIVE_OID_get_arcs(const RELATIVE_OID_t *st, asn_oid_arc_t *arcs,
                      size_t arcs_count) {
    size_t num_arcs = 0;
    size_t off;

    if(!st || !st->buf) {
        errno = EINVAL;
        return -1;
    }

    for(off = 0;;) {
        asn_oid_arc_t arc;
        ssize_t rd = OBJECT_IDENTIFIER_get_single_arc(st->buf + off,
                                                      st->size - off, &arc);
        if(rd < 0) {
            return -1;
        } else if(rd == 0) {
            /* No more arcs. */
            break;
        } else {
            off += rd;
            if(num_arcs < arcs_count) {
                arcs[num_arcs] = arc;
            }
            num_arcs++;
        }
    }

    if(off != st->size) {
        return -1;
    }

	return num_arcs;
}

int
RELATIVE_OID_set_arcs(RELATIVE_OID_t *st, const asn_oid_arc_t *arcs,
                      size_t arcs_count) {
    uint8_t *buf;
	uint8_t *bp;
    size_t size;
	size_t i;

	if(!st || !arcs) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Roughly estimate the maximum size necessary to encode these arcs.
	 */
    size = ((sizeof(asn_oid_arc_t) * CHAR_BIT + 6) / 7) * arcs_count;
    bp = buf = (uint8_t *)MALLOC(size + 1);
	if(!buf) {
		/* ENOMEM */
		return -1;
	}

	/*
	 * Encode the arcs.
	 */
    for(i = 0; i < arcs_count; i++) {
        ssize_t wrote = OBJECT_IDENTIFIER_set_single_arc(bp, size, arcs[i]);
        if(wrote <= 0) {
            FREEMEM(buf);
            return -1;
        }
        assert((size_t)wrote <= size);
        bp += wrote;
        size -= wrote;
    }

	/*
	 * Replace buffer.
	 */
	st->size = bp - buf;
	bp = st->buf;
	st->buf = buf;
	st->buf[st->size] = '\0';
	if(bp) FREEMEM(bp);

	return 0;
}


/*
 * Generate values from the list of interesting values, or just a random value.
 */
static asn_oid_arc_t
RELATIVE_OID__biased_random_arc() {
    static const uint16_t values[] = {0, 1, 127, 128, 129, 254, 255, 256};

    switch(asn_random_between(0, 2)) {
    case 0:
        return values[asn_random_between(
            0, sizeof(values) / sizeof(values[0]) - 1)];
    case 1:
        return asn_random_between(0, UINT_MAX);
    case 2:
    default:
        return UINT_MAX;
    }
}

asn_random_fill_result_t
RELATIVE_OID_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                         const asn_encoding_constraints_t *constraints,
                         size_t max_length) {
    asn_random_fill_result_t result_ok = {ARFILL_OK, 1};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    RELATIVE_OID_t *st;
    const int min_arcs = 1; /* A minimum of 1 arc is required */
    asn_oid_arc_t arcs[3];
    size_t arcs_len =
        asn_random_between(min_arcs, sizeof(arcs) / sizeof(arcs[0]));
    size_t i;

    (void)constraints;

    if(max_length < arcs_len) return result_skipped;

    if(*sptr) {
        st = *sptr;
    } else {
        st = CALLOC(1, sizeof(*st));
    }

    for(i = 0; i < arcs_len; i++) {
        arcs[i] = RELATIVE_OID__biased_random_arc();
    }

    if(RELATIVE_OID_set_arcs(st, arcs, arcs_len)) {
        if(st != *sptr) {
            ASN_STRUCT_FREE(*td, st);
        }
        return result_failed;
    }

    *sptr = st;

    result_ok.length = st->size;
    return result_ok;
}
