/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <UniversalString.h>
#include <UTF8String.h>

/*
 * UniversalString basic type description.
 */
static const ber_tlv_tag_t asn_DEF_UniversalString_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (28 << 2)),	/* [UNIVERSAL 28] IMPLICIT ...*/
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2))	/* ... OCTET STRING */
};
asn_OCTET_STRING_specifics_t asn_SPC_UniversalString_specs = {
	sizeof(UniversalString_t),
	offsetof(UniversalString_t, _asn_ctx),
	ASN_OSUBV_U32	/* 32-bits character */
};
static asn_per_constraints_t asn_DEF_UniversalString_per_constraints = {
	{ APC_CONSTRAINED, 32, 32, 0, 2147483647 },
	{ APC_SEMI_CONSTRAINED, -1, -1, 0, 0 },
	0, 0
};
asn_TYPE_operation_t asn_OP_UniversalString = {
	OCTET_STRING_free,
	UniversalString_print,      /* Convert into UTF8 and print */
	OCTET_STRING_compare,
	OCTET_STRING_decode_ber,
	OCTET_STRING_encode_der,
	UniversalString_decode_xer,	/* Convert from UTF-8 */
	UniversalString_encode_xer,	/* Convert into UTF-8 */
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
	OCTET_STRING_decode_uper,
	OCTET_STRING_encode_uper,
#endif	/* ASN_DISABLE_PER_SUPPORT */
	OCTET_STRING_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_UniversalString = {
	"UniversalString",
	"UniversalString",
	&asn_OP_UniversalString,
	asn_DEF_UniversalString_tags,
	sizeof(asn_DEF_UniversalString_tags)
	  / sizeof(asn_DEF_UniversalString_tags[0]) - 1,
	asn_DEF_UniversalString_tags,
	sizeof(asn_DEF_UniversalString_tags)
	  / sizeof(asn_DEF_UniversalString_tags[0]),
	{ 0, &asn_DEF_UniversalString_per_constraints, UniversalString_constraint },
	0, 0,	/* No members */
	&asn_SPC_UniversalString_specs
};

int
UniversalString_constraint(const asn_TYPE_descriptor_t *td, const void *sptr,
                           asn_app_constraint_failed_f *ctfailcb,
                           void *app_key) {
    const UniversalString_t *st = (const UniversalString_t *)sptr;

    if(st && st->buf) {
        if(st->size % 4) {
            ASN__CTFAIL(app_key, td, sptr,
                        "%s: invalid size %" ASN_PRI_SIZE " not divisible by 4 (%s:%d)",
                        td->name, st->size, __FILE__, __LINE__);
            return -1;
        }
    } else {
        ASN__CTFAIL(app_key, td, sptr, "%s: value not given (%s:%d)", td->name,
                    __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

static ssize_t
UniversalString__dump(const UniversalString_t *st,
		asn_app_consume_bytes_f *cb, void *app_key) {
	char scratch[128];			/* Scratchpad buffer */
	char *p = scratch;
	ssize_t wrote = 0;
	uint8_t *ch;
	uint8_t *end;

	ch = st->buf;
	end = (st->buf + st->size);
	for(end -= 3; ch < end; ch += 4) {
		uint32_t wc =     (ch[0] << 24)
				| (ch[1] << 16)
				| (ch[2] << 8)
				|  ch[3];	/* 4 bytes */
		if(sizeof(scratch) - (p - scratch) < 6) {
			wrote += p - scratch;
			if(cb(scratch, p - scratch, app_key) < 0)
				return -1;
			p = scratch;
		}
		if(wc < 0x80) {
			*p++ = (char)wc;
		} else if(wc < 0x800) {
			*p++ = 0xc0 | ((wc >> 6));
			*p++ = 0x80 | ((wc & 0x3f));
		} else if(wc < 0x10000) {
			*p++ = 0xe0 | ((wc >> 12));
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | ((wc & 0x3f));
		} else if(wc < 0x200000) {
			*p++ = 0xf0 | ((wc >> 18));
			*p++ = 0x80 | ((wc >> 12) & 0x3f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | ((wc & 0x3f));
		} else if(wc < 0x4000000) {
			*p++ = 0xf8 | ((wc >> 24));
			*p++ = 0x80 | ((wc >> 18) & 0x3f);
			*p++ = 0x80 | ((wc >> 12) & 0x3f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | ((wc & 0x3f));
		} else {
			*p++ = 0xfc | ((wc >> 30) & 0x1);
			*p++ = 0x80 | ((wc >> 24) & 0x3f);
			*p++ = 0x80 | ((wc >> 18) & 0x3f);
			*p++ = 0x80 | ((wc >> 12) & 0x3f);
			*p++ = 0x80 | ((wc >> 6) & 0x3f);
			*p++ = 0x80 | ((wc & 0x3f));
		}
	}

	wrote += p - scratch;
	if(cb(scratch, p - scratch, app_key) < 0)
		return -1;

	return wrote;
}

asn_dec_rval_t
UniversalString_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                           const asn_TYPE_descriptor_t *td, void **sptr,
                           const char *opt_mname, const void *buf_ptr,
                           size_t size) {
    asn_dec_rval_t rc;

	rc = OCTET_STRING_decode_xer_utf8(opt_codec_ctx, td, sptr, opt_mname,
		buf_ptr, size);
	if(rc.code == RC_OK) {
		/*
		 * Now we have a whole string in UTF-8 format.
		 * Convert it into UCS-4.
		 */
		uint32_t *wcs;
		size_t wcs_len;
		UTF8String_t *st;
#ifndef	WORDS_BIGENDIAN
		int little_endian = 1;
#endif

		assert(*sptr);
		st = (UTF8String_t *)*sptr;
		assert(st->buf);
		wcs_len = UTF8String_to_wcs(st, 0, 0);

		wcs = (uint32_t *)MALLOC(4 * (wcs_len + 1));
		if(wcs == 0 || UTF8String_to_wcs(st, wcs, wcs_len) != wcs_len) {
			rc.code = RC_FAIL;
			rc.consumed = 0;
			return rc;
		} else {
			wcs[wcs_len] = 0;	/* nul-terminate */
		}

#ifndef	WORDS_BIGENDIAN
		if(*(char *)&little_endian) {
			/* Swap byte order in encoding */
			uint32_t *wc = wcs;
			uint32_t *wc_end = wcs + wcs_len;
			for(; wc < wc_end; wc++) {
				/* *wc = htonl(*wc); */
				uint32_t wch = *wc;
				*((uint8_t *)wc + 0) = wch >> 24;
				*((uint8_t *)wc + 1) = wch >> 16;
				*((uint8_t *)wc + 2) = wch >> 8;
				*((uint8_t *)wc + 3) = wch;
			}
		}
#endif	/* WORDS_BIGENDIAN */

		FREEMEM(st->buf);
		st->buf = (uint8_t *)wcs;
		st->size = 4 * wcs_len;
	}
	return rc;
}

asn_enc_rval_t
UniversalString_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
                           int ilevel, enum xer_encoder_flags_e flags,
                           asn_app_consume_bytes_f *cb, void *app_key) {
    const UniversalString_t *st = (const UniversalString_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;
	(void)flags;

	if(!st || !st->buf)
		ASN__ENCODE_FAILED;

	er.encoded = UniversalString__dump(st, cb, app_key);
	if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

int
UniversalString_print(const asn_TYPE_descriptor_t *td, const void *sptr,
                      int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
    const UniversalString_t *st = (const UniversalString_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(!st || !st->buf) return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	if(UniversalString__dump(st, cb, app_key) < 0)
		return -1;

	return 0;
}

