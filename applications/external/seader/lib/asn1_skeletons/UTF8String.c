/*-
 * Copyright (c) 2003, 2004, 2006 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <UTF8String.h>

/*
 * UTF8String basic type description.
 */
static const ber_tlv_tag_t asn_DEF_UTF8String_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (12 << 2)),	/* [UNIVERSAL 12] IMPLICIT ...*/
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2)),	/* ... OCTET STRING */
};
asn_TYPE_operation_t asn_OP_UTF8String = {
	OCTET_STRING_free,
	UTF8String_print,
	OCTET_STRING_compare,
	OCTET_STRING_decode_ber,    /* Implemented in terms of OCTET STRING */
	OCTET_STRING_encode_der,
	OCTET_STRING_decode_xer_utf8,
	OCTET_STRING_encode_xer_utf8,
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
	UTF8String_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_UTF8String = {
	"UTF8String",
	"UTF8String",
	&asn_OP_UTF8String,
	asn_DEF_UTF8String_tags,
	sizeof(asn_DEF_UTF8String_tags)
	  / sizeof(asn_DEF_UTF8String_tags[0]) - 1,
	asn_DEF_UTF8String_tags,
	sizeof(asn_DEF_UTF8String_tags)
	  / sizeof(asn_DEF_UTF8String_tags[0]),
	{ 0, 0, UTF8String_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * This is the table of length expectations.
 * The second half of this table is only applicable to the long sequences.
 */
static const int UTF8String_ht[2][16] = {
	{ /* 0x0 ... 0x7 */
	  /* 0000..0111 */
	  1, 1, 1, 1, 1, 1, 1, 1,
	  /* 1000..1011(0), 1100..1101(2), 1110(3), 1111(-1) */
	  0, 0, 0, 0, 2, 2, 3, -1 },
	{ /* 0xF0 .. 0xF7 */
	  /* 11110000..11110111 */
	  4, 4, 4, 4, 4, 4, 4, 4,
	  5, 5, 5, 5, 6, 6, -1, -1 }
};
static const int32_t UTF8String_mv[7] = { 0, 0,
	0x00000080,
	0x00000800,
	0x00010000,
	0x00200000,
	0x04000000
};

/* Internal aliases for return codes */
#define	U8E_TRUNC	-1	/* UTF-8 sequence truncated */
#define	U8E_ILLSTART	-2	/* Illegal UTF-8 sequence start */
#define	U8E_NOTCONT	-3	/* Continuation expectation failed */
#define	U8E_NOTMIN	-4	/* Not minimal length encoding */
#define	U8E_EINVAL	-5	/* Invalid arguments */

int
UTF8String_constraint(const asn_TYPE_descriptor_t *td, const void *sptr,
                      asn_app_constraint_failed_f *ctfailcb, void *app_key) {
    ssize_t len = UTF8String_length((const UTF8String_t *)sptr);
	switch(len) {
	case U8E_EINVAL:
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given", td->name);
		break;
	case U8E_TRUNC:
		ASN__CTFAIL(app_key, td, sptr,
			"%s: truncated UTF-8 sequence (%s:%d)",
			td->name, __FILE__, __LINE__);
		break;
	case U8E_ILLSTART:
		ASN__CTFAIL(app_key, td, sptr,
			"%s: UTF-8 illegal start of encoding (%s:%d)",
			td->name, __FILE__, __LINE__);
		break;
	case U8E_NOTCONT:
		ASN__CTFAIL(app_key, td, sptr,
			"%s: UTF-8 not continuation (%s:%d)",
			td->name, __FILE__, __LINE__);
		break;
	case U8E_NOTMIN:
		ASN__CTFAIL(app_key, td, sptr,
			"%s: UTF-8 not minimal sequence (%s:%d)",
			td->name, __FILE__, __LINE__);
		break;
	}
	return (len < 0) ? -1 : 0;
}

static ssize_t
UTF8String__process(const UTF8String_t *st, uint32_t *dst, size_t dstlen) {
	size_t length;
	uint8_t *buf = st->buf;
	uint8_t *end = buf + st->size;
	uint32_t *dstend = dst + dstlen;

	for(length = 0; buf < end; length++) {
		int ch = *buf;
		uint8_t *cend;
		int32_t value;
		int want;

		/* Compute the sequence length */
		want = UTF8String_ht[0][ch >> 4];
		switch(want) {
		case -1:
			/* Second half of the table, long sequence */
			want = UTF8String_ht[1][ch & 0x0F];
			if(want != -1) break;
			/* Fall through */
		case 0:
			return U8E_ILLSTART;
		}

		/* assert(want >= 1 && want <= 6) */

		/* Check character sequence length */
		if(buf + want > end) return U8E_TRUNC;

		value = ch & (0xff >> want);
		cend = buf + want;
		for(buf++; buf < cend; buf++) {
			ch = *buf;
			if(ch < 0x80 || ch > 0xbf) return U8E_NOTCONT;
			value = (value << 6) | (ch & 0x3F);
		}
		if(value < UTF8String_mv[want])
			return U8E_NOTMIN;
		if(dst < dstend)
			*dst++ = value;	/* Record value */
	}

	if(dst < dstend) *dst = 0;	/* zero-terminate */

	return length;
}


ssize_t
UTF8String_length(const UTF8String_t *st) {
	if(st && st->buf) {
		return UTF8String__process(st, 0, 0);
	} else {
		return U8E_EINVAL;
	}
}

size_t
UTF8String_to_wcs(const UTF8String_t *st, uint32_t *dst, size_t dstlen) {
	if(st && st->buf) {
		ssize_t ret = UTF8String__process(st, dst, dstlen);
		return (ret < 0) ? 0 : ret;
	} else {
		return 0;
	}
}

int
UTF8String_print(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
                 asn_app_consume_bytes_f *cb, void *app_key) {
    const UTF8String_t *st = (const UTF8String_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(st && st->buf) {
		return (cb(st->buf, st->size, app_key) < 0) ? -1 : 0;
	} else {
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;
	}
}


/*
 * Biased function for randomizing UTF-8 sequences.
 */
static size_t
UTF8String__random_char(uint8_t *b, size_t size) {
    static const struct rnd_value {
        const char *value;
        size_t size;
    } values[] = {{"\0", 1},
                  {"\x01", 1},
                  {"\x7f", 1},
                  {"\xc2\xa2", 2},
                  {"\xe2\x82\xac", 3},
                  {"\xf0\x90\x8d\x88", 4},
                  {"\xf4\x8f\xbf\xbf", 4}};

    const struct rnd_value *v;
    size_t max_idx = 0;

    switch(size) {
    case 0:
        assert(size != 0);
        return 0;
    case 1:
        max_idx = 2;
        break;
    case 2:
        max_idx = 3;
        break;
    default:
    case 4:
        max_idx = sizeof(values) / sizeof(values[0]) - 1;
        break;
    }

    v = &values[asn_random_between(0, max_idx)];
    memcpy(b, v->value, v->size);
    return v->size;
}

asn_random_fill_result_t
UTF8String_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                       const asn_encoding_constraints_t *constraints,
                       size_t max_length) {
    asn_random_fill_result_t result_ok = {ARFILL_OK, 1};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    uint8_t *buf;
    uint8_t *bend;
    uint8_t *b;
    size_t rnd_len;
    size_t idx;
    UTF8String_t *st;

    if(max_length == 0 && !*sptr) return result_skipped;

    /* Figure out how far we should go */
    rnd_len = OCTET_STRING_random_length_constrained(td, constraints,
                                                     max_length / 4);

    buf = CALLOC(4, rnd_len + 1);
    if(!buf) return result_failed;

    bend = &buf[4 * rnd_len];

    for(b = buf, idx = 0; idx < rnd_len; idx++) {
        b += UTF8String__random_char(b, (bend - b));
    }
    *(uint8_t *)b = 0;

    if(*sptr) {
        st = *sptr;
        FREEMEM(st->buf);
    } else {
        st = (OCTET_STRING_t *)(*sptr = CALLOC(1, sizeof(UTF8String_t)));
        if(!st) {
            FREEMEM(buf);
            return result_failed;
        }
    }

    st->buf = buf;
    st->size = b - buf;

    assert(UTF8String_length(st) == (ssize_t)rnd_len);

    return result_ok;
}
