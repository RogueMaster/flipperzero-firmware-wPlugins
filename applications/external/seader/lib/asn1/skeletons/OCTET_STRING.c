/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <OCTET_STRING.h>
#include <BIT_STRING.h>	/* for .bits_unused usage */
#include <errno.h>

/*
 * OCTET STRING basic type description.
 */
static const ber_tlv_tag_t asn_DEF_OCTET_STRING_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2))
};
asn_TYPE_operation_t asn_OP_OCTET_STRING = {
	OCTET_STRING_free,
	OCTET_STRING_print,	/* OCTET STRING generally means a non-ascii sequence */
	OCTET_STRING_compare,
	OCTET_STRING_decode_ber,
	OCTET_STRING_encode_der,
#ifdef  ASN_DISABLE_XER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_xer_hex,
	OCTET_STRING_encode_xer,
#endif  /* ASN_DISABLE_XER_SUPPORT */
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_oer,
	OCTET_STRING_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_uper,	/* Unaligned PER decoder */
	OCTET_STRING_encode_uper,	/* Unaligned PER encoder */
#endif	/* ASN_DISABLE_PER_SUPPORT */
#ifdef  ASN_DISABLE_RANDOM_FILL
	0,
#else
	OCTET_STRING_random_fill,
#endif  /* ASN_DISABLE_RANDOM_FILL */
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_OCTET_STRING = {
	"OCTET STRING",		/* Canonical name */
	"OCTET_STRING",		/* XML tag name */
	&asn_OP_OCTET_STRING,
	asn_DEF_OCTET_STRING_tags,
	sizeof(asn_DEF_OCTET_STRING_tags)
		/ sizeof(asn_DEF_OCTET_STRING_tags[0]),
	asn_DEF_OCTET_STRING_tags,	/* Same as above */
	sizeof(asn_DEF_OCTET_STRING_tags)
		/ sizeof(asn_DEF_OCTET_STRING_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

#undef	ADVANCE
#define	ADVANCE(num_bytes)	do {				\
		size_t num = (num_bytes);			\
		buf_ptr = ((const char *)buf_ptr) + num;	\
		size -= num;					\
		consumed_myself += num;				\
	} while(0)

#undef	RETURN
#define	RETURN(_code)	do {					\
		asn_dec_rval_t tmprval;					\
		tmprval.code = _code;				\
		tmprval.consumed = consumed_myself;			\
		return tmprval;						\
	} while(0)

#undef	APPEND
#define	APPEND(bufptr, bufsize)	do {				\
		size_t _bs = (bufsize);				\
		size_t bsize = st->size;			\
		uint8_t *p = (uint8_t *)REALLOC(st->buf, bsize + _bs + 1); \
		if(!p) RETURN(RC_FAIL);				\
		st->buf = p;					\
		memcpy(st->buf + bsize, bufptr, _bs);		\
		st->size = bsize + _bs;				\
		st->buf[st->size] = '\0';			\
	} while(0)

/*
 * Decode OCTET STRING type.
 */
asn_dec_rval_t
OCTET_STRING_decode_ber(const asn_codec_ctx_t *opt_codec_ctx,
                        const asn_TYPE_descriptor_t *td, void **sptr,
                        const void *buf_ptr, size_t size, int tag_mode) {
	const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	uint8_t *st_buf;
	size_t st_size;
	OCTET_STRING_t *st = (OCTET_STRING_t *)*sptr;
	asn_dec_rval_t rval;
	ber_tlv_len_t length;
	ssize_t consumed_myself = 0;

	if(st == NULL) {
		st = (OCTET_STRING_t *)(*sptr = CALLOC(1, specs->struct_size));
		if(st == NULL) RETURN(RC_FAIL);
	}

	ASN_DEBUG("Decoding %s as OCTET STRING (tm=%d)",
		td->name, tag_mode);

	/*
	 * Check tags.
	 */
	rval = ber_check_tags(opt_codec_ctx, td, 0, buf_ptr, size,
			tag_mode, 0, &length, 0);
	if(rval.code != RC_OK)
		return rval;

	ADVANCE(rval.consumed);

	/*
	 * Refuse to decode things that are too big.
	 */
	if(length > 0 && (size_t)length > size)
		RETURN(RC_WMORE);

	if(length != -1) {
		/*
		 * If we have a fixed length, just read it.
		 */
		st_buf = (uint8_t *)MALLOC(length + 1);
		if(st_buf == NULL) RETURN(RC_FAIL);
		memcpy(st_buf, buf_ptr, length);
		st_buf[length] = '\0';
		st_size = length;
		ADVANCE(length);
	} else {
		/*
		 * Indefinite length encoding.
		 */
		st_buf = NULL;
		st_size = 0;
		for(;;) {
			ber_tlv_tag_t t;
			ber_tlv_len_t l;

			ssize_t t_len = ber_fetch_tag(buf_ptr, size, &t);
			if(t_len == -1) {
				FREEMEM(st_buf);
				RETURN(RC_FAIL);
			} else if(t_len == 0) {
				FREEMEM(st_buf);
				RETURN(RC_WMORE);
			}

			if(t == 0) {
				/* End of content octets */
				t_len = ber_fetch_tag(buf_ptr, size, &t);
				if(t_len != 2 || ((const uint8_t *)buf_ptr)[0] != 0 || ((const uint8_t *)buf_ptr)[1] != 0) {
					FREEMEM(st_buf);
					RETURN(RC_FAIL);
				}
				ADVANCE(2);
				break;
			}

			rval = ber_check_tags(opt_codec_ctx, td, 0, buf_ptr, size,
					-1, 0, &l, 0);
			if(rval.code != RC_OK) {
				FREEMEM(st_buf);
				return rval;
			}

			if(l == -1) {
				/* Indefinite length not allowed here */
				FREEMEM(st_buf);
				RETURN(RC_FAIL);
			}

			if(l > (ber_tlv_len_t)(size - rval.consumed)) {
				FREEMEM(st_buf);
				RETURN(RC_WMORE);
			}

			uint8_t *new_buf = (uint8_t *)REALLOC(st_buf, st_size + l + 1);
			if(new_buf == NULL) {
				FREEMEM(st_buf);
				RETURN(RC_FAIL);
			}
			st_buf = new_buf;
			memcpy(st_buf + st_size, (const char *)buf_ptr + rval.consumed, l);
			st_size += l;
			st_buf[st_size] = '\0';
			ADVANCE(rval.consumed + l);
		}
	}

	FREEMEM(st->buf);
	st->buf = st_buf;
	st->size = st_size;

	RETURN(RC_OK);
}

asn_enc_rval_t
OCTET_STRING_encode_der(const asn_TYPE_descriptor_t *td, const void *sptr,
                        int tag_mode, ber_tlv_tag_t tag,
                        asn_app_consume_bytes_f *cb, void *app_key) {
    const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
	asn_enc_rval_t erval;

	ASN_DEBUG("Encoding %s as OCTET STRING (tm=%d)",
		td->name, tag_mode);

	erval.encoded = der_write_tags(td, st->size, tag_mode, 0, tag,
		cb, app_key);
	if(erval.encoded == -1) {
		erval.failed_type = td;
		erval.structure_ptr = sptr;
		return erval;
	}

	if(cb && st->buf) {
		if(cb(st->buf, st->size, app_key) < 0) {
			erval.encoded = -1;
			erval.failed_type = td;
			erval.structure_ptr = sptr;
			return erval;
		}
	} else {
		assert(st->buf || st->size == 0);
	}

	erval.encoded += st->size;
	ASN__ENCODED_OK(erval);
}

#ifndef ASN_DISABLE_XER_SUPPORT

static const char *OCTET_STRING__xer_escape_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00 .. 0x0f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 .. 0x1f */
    0, 0, 0, 0, 0, 0, "&amp;", 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 .. 0x2f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "&lt;", 0, "&gt;", 0, /* 0x30 .. 0x3f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 .. 0x4f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 .. 0x5f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 .. 0x6f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 .. 0x7f */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0};

static size_t
OCTET_STRING__xer_escape(const void *buf, size_t size,
                         asn_app_consume_bytes_f *cb, void *app_key) {
    size_t i, last_idx;
    const uint8_t *b = (const uint8_t *)buf;
    size_t wrote = 0;

    for(last_idx = i = 0; i < size; i++) {
        const char *esc = OCTET_STRING__xer_escape_table[b[i]];
        if(esc) {
            if(i > last_idx) {
                if(cb(b + last_idx, i - last_idx, app_key) < 0) return -1;
                wrote += i - last_idx;
            }
            size_t esc_len = strlen(esc);
            if(cb(esc, esc_len, app_key) < 0) return -1;
            wrote += esc_len;
            last_idx = i + 1;
        }
    }

    if(i > last_idx) {
        if(cb(b + last_idx, i - last_idx, app_key) < 0) return -1;
        wrote += i - last_idx;
    }

    return wrote;
}

asn_enc_rval_t
OCTET_STRING_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
                        int ilevel, enum xer_encoder_flags_e flags,
                        asn_app_consume_bytes_f *cb, void *app_key) {
    const char * const h2c = "0123456789ABCDEF";
	const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
	asn_enc_rval_t er = {0, 0, 0};
	char scratch[128];
	uint8_t *buf;
	uint8_t *end;
	uint8_t *p;

	(void)ilevel;

	if(!st || !st->buf)
		ASN__ENCODE_FAILED;

	er.encoded = 0;

	if(flags & XER_F_CANONICAL) {
		/* Hexadecimal path */
		for(buf = st->buf, end = st->buf + st->size; buf < end; ) {
			p = (uint8_t *)scratch;
			while(p < (uint8_t *)scratch + sizeof(scratch) - 3 && buf < end) {
				*p++ = h2c[*buf >> 4];
				*p++ = h2c[*buf & 0x0F];
				buf++;
			}
			if(cb(scratch, p - (uint8_t *)scratch, app_key) < 0)
				ASN__ENCODE_FAILED;
			er.encoded += p - (uint8_t *)scratch;
		}
	} else {
		/* Textual path */
		ssize_t wrote = OCTET_STRING__xer_escape(st->buf, st->size, cb, app_key);
		if(wrote == -1) ASN__ENCODE_FAILED;
		er.encoded = wrote;
	}

	ASN__ENCODED_OK(er);
cb_failed:
	ASN__ENCODE_FAILED;
}

asn_enc_rval_t
OCTET_STRING_encode_xer_utf8(const asn_TYPE_descriptor_t *td, const void *sptr,
                             int ilevel, enum xer_encoder_flags_e flags,
                             asn_app_consume_bytes_f *cb, void *app_key) {
    const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
    asn_enc_rval_t er = {0, 0, 0};

    (void)ilevel;
    (void)flags;

    if(!st || !st->buf) ASN__ENCODE_FAILED;

    ssize_t wrote = OCTET_STRING__xer_escape(st->buf, st->size, cb, app_key);
    if(wrote == -1) ASN__ENCODE_FAILED;
    er.encoded = wrote;

    ASN__ENCODED_OK(er);
cb_failed:
    ASN__ENCODE_FAILED;
}

static enum xer_pbd_rval
OCTET_STRING__handle_control_chars(void *struct_key, const void *chunk_buf, size_t chunk_size) {
	OCTET_STRING_t *st = (OCTET_STRING_t *)struct_key;
	const char *p = (const char *)chunk_buf;
	const char *end = p + chunk_size;

	for(; p < end; p++) {
		if(*(const unsigned char *)p < 32) {
			/* Control character detected */
			return XPBD_BROKEN_ENCODING;
		}
	}

	APPEND(chunk_buf, chunk_size);
	return XPBD_BODY_CONSUMED;
}

static int
OCTET_STRING__convert_entrefs(void *struct_key, const char *entname, size_t entlen) {
	OCTET_STRING_t *st = (OCTET_STRING_t *)struct_key;
	char ch;

	if(entlen == 3 && memcmp(entname, "lt", 3) == 0) ch = '<';
	else if(entlen == 3 && memcmp(entname, "gt", 3) == 0) ch = '>';
	else if(entlen == 4 && memcmp(entname, "amp", 4) == 0) ch = '&';
	else return -1;

	APPEND(&ch, 1);
	return 0;
}

static asn_dec_rval_t
OCTET_STRING__decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                         const asn_TYPE_descriptor_t *td, void **sptr,
                         const char *opt_mname, const void *buf_ptr,
                         size_t size,
                         enum xer_pbd_rval (*body_receiver)(void *, const void *, size_t),
                         int (*entity_ref_handler)(void *, const char *, size_t)) {
    const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	OCTET_STRING_t *st = (OCTET_STRING_t *)*sptr;
	asn_dec_rval_t rval;
	asn_struct_ctx_t s_ctx;
	ssize_t consumed_myself = 0;
	const char *xml_tag = opt_mname ? opt_mname : td->xml_tag;

	if(st == NULL) {
		st = (OCTET_STRING_t *)(*sptr = CALLOC(1, specs->struct_size));
		if(st == NULL) RETURN(RC_FAIL);
	}

	memset(&s_ctx, 0, sizeof(s_ctx));
	rval = xer_decode_general(opt_codec_ctx, &s_ctx, st, xml_tag,
		buf_ptr, size, 
		NULL, // No unexpected tags
		(ssize_t (*)(void *, const void *, size_t, int))body_receiver
	);

	RETURN(RC_OK);
}

asn_dec_rval_t
OCTET_STRING_decode_xer_hex(const asn_codec_ctx_t *opt_codec_ctx,
                            const asn_TYPE_descriptor_t *td, void **sptr,
                            const char *opt_mname, const void *buf_ptr,
                            size_t size) {

    return OCTET_STRING__decode_xer(opt_codec_ctx, td, sptr, opt_mname,
		buf_ptr, size,
		OCTET_STRING__handle_control_chars,
		OCTET_STRING__convert_entrefs);
}

asn_dec_rval_t
OCTET_STRING_decode_xer_binary(const asn_codec_ctx_t *opt_codec_ctx,
                               const asn_TYPE_descriptor_t *td, void **sptr,
                               const char *opt_mname, const void *buf_ptr,
                               size_t size) {
    return OCTET_STRING__decode_xer(opt_codec_ctx, td, sptr, opt_mname,
		buf_ptr, size,
		OCTET_STRING__handle_control_chars,
		OCTET_STRING__convert_entrefs);
}

asn_dec_rval_t
OCTET_STRING_decode_xer_utf8(const asn_codec_ctx_t *opt_codec_ctx,
                             const asn_TYPE_descriptor_t *td, void **sptr,
                             const char *opt_mname, const void *buf_ptr,
                             size_t size) {
    return OCTET_STRING__decode_xer(opt_codec_ctx, td, sptr, opt_mname,
		buf_ptr, size,
		OCTET_STRING__handle_control_chars,
		OCTET_STRING__convert_entrefs);
}
#endif /* ASN_DISABLE_XER_SUPPORT */

int
OCTET_STRING_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
                     const void *bptr) {
    const asn_OCTET_STRING_specifics_t *specs = td->specifics;
    const OCTET_STRING_t *a = aptr;
    const OCTET_STRING_t *b = bptr;

    (void)specs;
    assert(!specs || specs->subvariant != ASN_OSUBV_BIT);

    if(a && b) {
        size_t common_prefix_size = a->size <= b->size ? a->size : b->size;
        int ret = memcmp(a->buf, b->buf, common_prefix_size);
        if(ret == 0) {
            /* Lexicographical comparison: the shortest string wins */
            if(a->size < b->size)
                return -1;
            else if(a->size > b->size)
                return 1;
            else
                return 0;
        }
        return ret;
    } else if(!a && !b) {
        return 0;
    } else if(!a) {
        return -1;
    } else {
        return 1;
    }
}

int
OCTET_STRING_print(const asn_TYPE_descriptor_t *td, const void *sptr,
                   int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
    const char * const h2c = "0123456789ABCDEF";
	const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
	char scratch[16 * 3 + 4];
	char *p = scratch;
	uint8_t *buf;
	uint8_t *end;
	size_t i;

	(void)td;	/* Unused argument */

	if(!st || !st->buf) return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	ilevel++;
	for(i = 0, buf = st->buf, end = st->buf + st->size; buf < end; i++, buf++) {
		*p++ = h2c[*buf >> 4];
		*p++ = h2c[*buf & 0x0F];
		*p++ = 0x20;
		if(i && (i % 16) == 0 && buf + 1 < end) {
			*p++ = 0x0a;
			for(int j = 0; j < ilevel; j++) *p++ = 0x09;
			if(cb(scratch, p - scratch, app_key) < 0)
				return -1;
			p = scratch;
		}
	}

	if(p != scratch) {
		p--;	/* Remove the last ' ' or '\n' */
		if(cb(scratch, p - scratch, app_key) < 0)
			return -1;
	}

	return 0;
}

int
OCTET_STRING_print_utf8(const asn_TYPE_descriptor_t *td, const void *sptr,
                        int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
    const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(st && st->buf)
		return (cb(st->buf, st->size, app_key) < 0) ? -1 : 0;
	else
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;
}

void
OCTET_STRING_free(const asn_TYPE_descriptor_t *td, void *sptr,
                  enum asn_struct_free_method method) {
	const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	OCTET_STRING_t *st = (OCTET_STRING_t *)sptr;

	if(!td || !st)
		return;

	ASN_DEBUG("Freeing %s as OCTET STRING", td->name);

	if(st->buf)
		FREEMEM(st->buf);

    switch(method) {
    case ASFM_FREE_EVERYTHING:
        FREEMEM(sptr);
        break;
    case ASFM_FREE_UNDERLYING:
        break;
    case ASFM_FREE_UNDERLYING_AND_RESET:
        memset(sptr, 0, specs->struct_size);
        break;
    }
}

#ifndef  ASN_DISABLE_PER_SUPPORT

static int
OCTET_STRING_per_get_characters(asn_per_data_t *po, uint8_t *buf,
                                size_t units, unsigned int bpc, unsigned int unit_bits,
                                long lb, long ub, const asn_per_constraints_t *pc) {
    uint8_t *p = buf;
    uint8_t *end = buf + units * bpc;

    for(; p < end; p += bpc) {
        intmax_t code;
        if(pc && pc->value.flags & APC_CONSTRAINED) {
            code = per_get_few_bits(po, pc->value.range_bits);
            if(code < 0) return -1;
            code += pc->value.lower_bound;
        } else {
            code = per_get_few_bits(po, unit_bits);
            if(code < 0) return -1;
        }
        if(bpc == 1) {
            *p = (uint8_t)code;
        } else if(bpc == 2) {
            p[0] = (uint8_t)(code >> 8);
            p[1] = (uint8_t)code;
        } else if(bpc == 4) {
            p[0] = (uint8_t)(code >> 24);
            p[1] = (uint8_t)(code >> 16);
            p[2] = (uint8_t)(code >> 8);
            p[3] = (uint8_t)code;
        }
    }

    return 0;
}

asn_dec_rval_t
OCTET_STRING_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                         const asn_TYPE_descriptor_t *td,
                         const asn_per_constraints_t *constraints, void **sptr,
                         asn_per_data_t *pd) {
    const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	const asn_per_constraints_t *pc = constraints
				? constraints
				: &td->encoding_constraints.per_constraints;
	const asn_per_constraint_t *csize;
	const asn_per_constraint_t *cvalue;
	OCTET_STRING_t *st = (OCTET_STRING_t *)*sptr;
	asn_dec_rval_t rval = { RC_OK, 0 };
	unsigned int bpc;	/* Bits per character */
	unsigned int unit_bits;
	int repeat;
	ssize_t len;

	(void)opt_codec_ctx;

	if(st == NULL) {
		st = (OCTET_STRING_t *)(*sptr = CALLOC(1, specs->struct_size));
		if(st == NULL) RETURN(RC_FAIL);
	}

	if(pc) {
		csize = &pc->size;
		cvalue = &pc->value;
	} else {
		csize = &asn_per_constraint_NOT_CONSTRAINED;
		cvalue = &asn_per_constraint_NOT_CONSTRAINED;
	}

	switch(specs->subvariant) {
	default:
	case ASN_OSUBV_ANY:
	case ASN_OSUBV_BIT:
		RETURN(RC_FAIL);
	case ASN_OSUBV_STR:
		bpc = 1;
		unit_bits = 8;
		break;
	case ASN_OSUBV_U16:
		bpc = 2;
		unit_bits = 16;
		break;
	case ASN_OSUBV_U32:
		bpc = 4;
		unit_bits = 32;
		break;
	}

	/* Indefinite length? */
	if(csize->flags & APC_EXTENSIBLE) {
		int inext = per_get_few_bits(pd, 1);
		if(inext < 0) RETURN(RC_WMORE);
		if(inext) csize = &asn_per_constraint_NOT_CONSTRAINED;
	}

	if(csize->flags & APC_CONSTRAINED) {
		len = per_get_few_bits(pd, csize->range_bits);
		if(len < 0) RETURN(RC_WMORE);
		len += csize->lower_bound;
	} else {
		len = -1;
	}

	/* Pre-allocate buffer */
	if(len != -1) {
		st->buf = (uint8_t *)MALLOC(len * bpc + 1);
		if(!st->buf) RETURN(RC_FAIL);
		st->size = len * bpc;
		st->buf[st->size] = '\0';
	}

	if(len != -1) {
		if(OCTET_STRING_per_get_characters(pd, st->buf, len, bpc, unit_bits,
			cvalue->lower_bound, cvalue->upper_bound, pc))
			RETURN(RC_WMORE);
		RETURN(RC_OK);
	}

	/* Indefinite length */
	do {
		len = uper_get_length(pd, -1, 0, &repeat);
		if(len < 0) RETURN(RC_WMORE);
		uint8_t *new_buf = (uint8_t *)REALLOC(st->buf, st->size + len * bpc + 1);
		if(!new_buf) RETURN(RC_FAIL);
		st->buf = new_buf;
		if(OCTET_STRING_per_get_characters(pd, st->buf + st->size, len, bpc, unit_bits,
			cvalue->lower_bound, cvalue->upper_bound, pc))
			RETURN(RC_WMORE);
		st->size += len * bpc;
		st->buf[st->size] = '\0';
	} while(repeat);

	RETURN(RC_OK);
}

asn_enc_rval_t
OCTET_STRING_encode_uper(const asn_TYPE_descriptor_t *td,
                         const asn_per_constraints_t *constraints,
                         const void *sptr, asn_per_outp_t *po) {
    const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	const asn_per_constraints_t *pc = constraints
				? constraints
				: &td->encoding_constraints.per_constraints;
	const asn_per_constraint_t *csize;
	const asn_per_constraint_t *cvalue;
	const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
	asn_enc_rval_t er = { 0, 0, 0 };
	unsigned int bpc;	/* Bits per character */
	unsigned int unit_bits;
	size_t len;

	if(!st || (!st->buf && st->size)) ASN__ENCODE_FAILED;

	switch(specs->subvariant) {
	default:
	case ASN_OSUBV_ANY:
	case ASN_OSUBV_BIT:
		ASN__ENCODE_FAILED;
	case ASN_OSUBV_STR:
		bpc = 1;
		unit_bits = 8;
		break;
	case ASN_OSUBV_U16:
		bpc = 2;
		unit_bits = 16;
		break;
	case ASN_OSUBV_U32:
		bpc = 4;
		unit_bits = 32;
		break;
	}

	if(pc) {
		csize = &pc->size;
		cvalue = &pc->value;
	} else {
		csize = &asn_per_constraint_NOT_CONSTRAINED;
		cvalue = &asn_per_constraint_NOT_CONSTRAINED;
	}

	len = st->size / bpc;
	if(csize->flags & APC_CONSTRAINED) {
		if(len < (size_t)csize->lower_bound || len > (size_t)csize->upper_bound) {
			if(csize->flags & APC_EXTENSIBLE) {
				if(per_put_few_bits(po, 1, 1)) ASN__ENCODE_FAILED;
				csize = &asn_per_constraint_NOT_CONSTRAINED;
			} else {
				ASN__ENCODE_FAILED;
			}
		} else {
			if(csize->flags & APC_EXTENSIBLE) {
				if(per_put_few_bits(po, 0, 1)) ASN__ENCODE_FAILED;
			}
		}
	} else {
		if(csize->flags & APC_EXTENSIBLE) {
			if(per_put_few_bits(po, 0, 1)) ASN__ENCODE_FAILED;
		}
	}

	if(csize->flags & APC_CONSTRAINED) {
		if(per_put_few_bits(po, len - csize->lower_bound, csize->range_bits))
			ASN__ENCODE_FAILED;
		for(size_t i = 0; i < len; i++) {
			intmax_t code;
			if(bpc == 1) code = st->buf[i];
			else if(bpc == 2) code = (st->buf[2*i] << 8) | st->buf[2*i+1];
			else code = (st->buf[4*i] << 24) | (st->buf[4*i+1] << 16) | (st->buf[4*i+2] << 8) | st->buf[4*i+3];
			if(cvalue->flags & APC_CONSTRAINED) {
				if(per_put_few_bits(po, code - cvalue->lower_bound, cvalue->range_bits))
					ASN__ENCODE_FAILED;
			} else {
				if(per_put_few_bits(po, code, unit_bits))
					ASN__ENCODE_FAILED;
			}
		}
		ASN__ENCODED_OK(er);
	}

	/* Indefinite length */
	const uint8_t *buf = st->buf;
	const uint8_t *end = st->buf + st->size;
	while(buf < end) {
		int need_eom = 0;
		ssize_t may_encode = uper_put_length(po, (end - buf) / bpc, &need_eom);
		if(may_encode < 0) ASN__ENCODE_FAILED;
		for(ssize_t i = 0; i < may_encode; i++) {
			intmax_t code;
			if(bpc == 1) code = buf[i];
			else if(bpc == 2) code = (buf[2*i] << 8) | buf[2*i+1];
			else code = (buf[4*i] << 24) | (buf[4*i+1] << 16) | (buf[4*i+2] << 8) | buf[4*i+3];
			if(per_put_few_bits(po, code, unit_bits))
				ASN__ENCODE_FAILED;
		}
		buf += may_encode * bpc;
		if(need_eom && uper_put_length(po, 0, 0)) ASN__ENCODE_FAILED;
	}

	ASN__ENCODED_OK(er);
}

#endif	/* ASN_DISABLE_PER_SUPPORT */

#ifndef  ASN_DISABLE_OER_SUPPORT

asn_dec_rval_t
OCTET_STRING_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                        const asn_TYPE_descriptor_t *td,
                        const asn_oer_constraints_t *constraints, void **sptr,
                        const void *ptr, size_t size) {
    const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
    OCTET_STRING_t *st = (OCTET_STRING_t *)*sptr;
    const uint8_t *p = ptr;
    const uint8_t *end = p + size;
    size_t len;
    ssize_t len_len;

    (void)opt_codec_ctx;
    (void)constraints;

    if(st == NULL) {
        st = (OCTET_STRING_t *)(*sptr = CALLOC(1, specs->struct_size));
        if(st == NULL) ASN__DECODE_FAILED;
    }

    len_len = oer_fetch_length(p, end - p, &len);
    if(len_len == -1) ASN__DECODE_FAILED;
    if(len_len == 0) ASN__DECODE_STARVED;
    p += len_len;

    if((size_t)(end - p) < len) ASN__DECODE_STARVED;

    st->buf = (uint8_t *)MALLOC(len + 1);
    if(st->buf == NULL) ASN__DECODE_FAILED;
    memcpy(st->buf, p, len);
    st->buf[len] = '\0';
    st->size = len;

    asn_dec_rval_t ok = {RC_OK, len_len + len};
    return ok;
}

asn_enc_rval_t
OCTET_STRING_encode_oer(const asn_TYPE_descriptor_t *td,
                        const asn_oer_constraints_t *constraints,
                        const void *sptr, asn_app_consume_bytes_f *cb,
                        void *app_key) {
    const OCTET_STRING_t *st = (const OCTET_STRING_t *)sptr;
    asn_enc_rval_t er = {0, 0, 0};
    ssize_t len_len;

    (void)constraints;

    if(!st) ASN__ENCODE_FAILED;

    len_len = oer_serialize_length(st->size, cb, app_key);
    if(len_len == -1) ASN__ENCODE_FAILED;

    if(cb(st->buf, st->size, app_key) < 0) ASN__ENCODE_FAILED;

    er.encoded = len_len + st->size;
    ASN__ENCODED_OK(er);
}

#endif /* ASN_DISABLE_OER_SUPPORT */

int
OCTET_STRING_fromBuf(OCTET_STRING_t *st, const char *str, int len) {
	uint8_t *buf;

	if(st == NULL) return -1;
	if(str == NULL) {
		FREEMEM(st->buf);
		st->buf = 0;
		st->size = 0;
		return 0;
	}

	if(len < 0) len = strlen(str);

	buf = (uint8_t *)MALLOC(len + 1);
	if(buf == NULL) return -1;

	memcpy(buf, str, len);
	buf[len] = '\0';

	FREEMEM(st->buf);
	st->buf = buf;
	st->size = len;

	return 0;
}

OCTET_STRING_t *
OCTET_STRING_new_fromBuf(const asn_TYPE_descriptor_t *td, const char *str, int len) {
	const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
	OCTET_STRING_t *st = (OCTET_STRING_t *)CALLOC(1, specs->struct_size);
	if(st && OCTET_STRING_fromBuf(st, str, len)) {
		FREEMEM(st);
		st = NULL;
	}
	return st;
}

asn_OCTET_STRING_specifics_t asn_SPC_OCTET_STRING_specs = {
	sizeof(OCTET_STRING_t),
	offsetof(OCTET_STRING_t, _asn_ctx),
	ASN_OSUBV_STR
};

#ifndef ASN_DISABLE_RANDOM_FILL
static uint32_t
OCTET_STRING__random_char(unsigned long lb, unsigned long ub) {
    assert(lb <= ub);
    switch(asn_random_between(0, 16)) {
    case 0:
        if(lb < ub) return lb + 1;
        /* Fall through */
    case 1:
        return lb;
    case 2:
        if(lb < ub) return ub - 1;
        /* Fall through */
    case 3:
        return ub;
    default:
        return asn_random_between(lb, ub);
    }
}

size_t
OCTET_STRING_random_length_constrained(
    const asn_TYPE_descriptor_t *td,
    const asn_encoding_constraints_t *constraints, size_t max_length) {
    const unsigned lengths[] = {0,     1,     2,     3,     4,     8,
                                126,   127,   128,   16383, 16384, 16385,
                                65534, 65535, 65536, 65537};
    size_t rnd_len;

    /* Figure out how far we should go */
    rnd_len = lengths[asn_random_between(
        0, sizeof(lengths) / sizeof(lengths[0]) - 1)];

    if(!constraints || !constraints->per_constraints)
        constraints = &td->encoding_constraints;
    if(constraints->per_constraints) {
        const asn_per_constraint_t *pc = &constraints->per_constraints->size;
        if(pc->flags & APC_CONSTRAINED) {
            long suggested_upper_bound = pc->upper_bound < (ssize_t)max_length
                                             ? pc->upper_bound
                                             : (ssize_t)max_length;
            if(max_length <= (size_t)pc->lower_bound) {
                return pc->lower_bound;
            }
            if(pc->flags & APC_EXTENSIBLE) {
                switch(asn_random_between(0, 5)) {
                case 0:
                    if(pc->lower_bound > 0) {
                        rnd_len = pc->lower_bound - 1;
                        break;
                    }
                    /* Fall through */
                case 1:
                    rnd_len = pc->upper_bound + 1;
                    break;
                case 2:
                    /* Keep rnd_len from the table */
                    if(rnd_len <= max_length) {
                        break;
                    }
                    /* Fall through */
                default:
                    rnd_len = asn_random_between(pc->lower_bound,
                                                 suggested_upper_bound);
                }
            } else {
                rnd_len =
                    asn_random_between(pc->lower_bound, suggested_upper_bound);
            }
        } else {
            rnd_len = asn_random_between(0, max_length);
        }
    } else if(rnd_len > max_length) {
        rnd_len = asn_random_between(0, max_length);
    }

    return rnd_len;
}

asn_random_fill_result_t
OCTET_STRING_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                         const asn_encoding_constraints_t *constraints,
                         size_t max_length) {
	const asn_OCTET_STRING_specifics_t *specs = td->specifics
				? (const asn_OCTET_STRING_specifics_t *)td->specifics
				: &asn_SPC_OCTET_STRING_specs;
    asn_random_fill_result_t result_ok = {ARFILL_OK, 1};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    unsigned int unit_bytes = 1;
    unsigned long clb = 0;  /* Lower bound on char */
    unsigned long cub = 255;  /* Higher bound on char value */
    uint8_t *buf;
    uint8_t *bend;
    uint8_t *b;
    size_t rnd_len;
    OCTET_STRING_t *st;

    if(max_length == 0 && !*sptr) return result_skipped;

    switch(specs->subvariant) {
    default:
    case ASN_OSUBV_ANY:
        return result_failed;
    case ASN_OSUBV_BIT:
        /* Handled by BIT_STRING itself. */
        return result_failed;
    case ASN_OSUBV_STR:
        unit_bytes = 1;
        clb = 0;
        cub = 255;
        break;
    case ASN_OSUBV_U16:
        unit_bytes = 2;
        clb = 0;
        cub = 65535;
        break;
    case ASN_OSUBV_U32:
        unit_bytes = 4;
        clb = 0;
        cub = 0x10FFFF;
        break;
    }

    if(!constraints || !constraints->per_constraints)
        constraints = &td->encoding_constraints;
    if(constraints->per_constraints) {
        const asn_per_constraint_t *pc = &constraints->per_constraints->value;
        if(pc->flags & APC_SEMI_CONSTRAINED) {
            clb = pc->lower_bound;
        } else if(pc->flags & APC_CONSTRAINED) {
            clb = pc->lower_bound;
            cub = pc->upper_bound;
        }
    }

    rnd_len =
        OCTET_STRING_random_length_constrained(td, constraints, max_length);

    buf = (uint8_t *)CALLOC(unit_bytes, rnd_len + 1);
    if(!buf) return result_failed;

    bend = &buf[unit_bytes * rnd_len];

    switch(unit_bytes) {
    case 1:
        for(b = buf; b < bend; b += unit_bytes) {
            *(uint8_t *)b = OCTET_STRING__random_char(clb, cub);
        }
        *(uint8_t *)b = 0;
        break;
    case 2:
        for(b = buf; b < bend; b += unit_bytes) {
            uint32_t code = OCTET_STRING__random_char(clb, cub);
            b[0] = code >> 8;
            b[1] = code;
        }
        *(uint16_t *)b = 0;
        break;
    case 4:
        for(b = buf; b < bend; b += unit_bytes) {
            uint32_t code = OCTET_STRING__random_char(clb, cub);
            b[0] = code >> 24;
            b[1] = code >> 16;
            b[2] = code >> 8;
            b[3] = code;
        }
        *(uint32_t *)b = 0;
        break;
    }

    if(*sptr) {
        st = *sptr;
        FREEMEM(st->buf);
    } else {
        st = (OCTET_STRING_t *)(*sptr = CALLOC(1, specs->struct_size));
        if(!st) {
            FREEMEM(buf);
            return result_failed;
        }
    }

    st->buf = buf;
    st->size = unit_bytes * rnd_len;

    result_ok.length = st->size;
    return result_ok;
}
#endif /* ASN_DISABLE_RANDOM_FILL */
