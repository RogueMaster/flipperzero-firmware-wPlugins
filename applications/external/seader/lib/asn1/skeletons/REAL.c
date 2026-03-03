/*-
 * Copyright (c) 2004-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#define	_ISOC99_SOURCE		/* For ilogb() and quiet NAN */
#ifndef _BSD_SOURCE
#define	_BSD_SOURCE		/* To reintroduce finite(3) */
#endif
#include <asn_internal.h>
#if	defined(__alpha)
#include <sys/resource.h>	/* For INFINITY */
#endif
#include <stdlib.h>	/* for strtod(3) */
#include <math.h>
#include <float.h>
#include <errno.h>
#include <REAL.h>
#include <OCTET_STRING.h>

#undef	INT_MAX
#define	INT_MAX	((int)(((unsigned int)-1) >> 1))

#if	!(defined(NAN) || defined(INFINITY))
static volatile double real_zero CC_NOTUSED = 0.0;
#endif
#ifndef	NAN
#define	NAN	(0.0/0.0)
#endif
#ifndef	INFINITY
#define	INFINITY	(1.0/0.0)
#endif

#if defined(__clang__)
/*
 * isnan() is defined using generic selections and won't compile in
 * strict C89 mode because of too fancy system's standard library.
 * However, prior to C11 the math had a perfectly working isnan()
 * in the math library.
 * Disable generic selection warning so we can test C89 mode with newer libc.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
static int asn_isnan(double d) {
    return isnan(d);
}
static int asn_isfinite(double d) {
#ifdef isfinite
    return isfinite(d);  /* ISO C99 */
#else
    return finite(d);    /* Deprecated on Mac OS X 10.9 */
#endif
}
#pragma clang diagnostic pop
#else   /* !clang */
#define asn_isnan(v)    isnan(v)
#ifdef isfinite
#define asn_isfinite(d)   isfinite(d)  /* ISO C99 */
#else
#define asn_isfinite(d)   finite(d)    /* Deprecated on Mac OS X 10.9 */
#endif
#endif  /* clang */

/*
 * REAL basic type description.
 */
static const ber_tlv_tag_t asn_DEF_REAL_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (9 << 2))
};
asn_TYPE_operation_t asn_OP_REAL = {
	ASN__PRIMITIVE_TYPE_free,
	REAL_print,
	REAL_compare,
	ber_decode_primitive,
	der_encode_primitive,
	REAL_decode_xer,
	REAL_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	REAL_decode_oer,
	REAL_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	REAL_decode_uper,
	REAL_encode_uper,
#endif	/* ASN_DISABLE_PER_SUPPORT */
	REAL_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_REAL = {
	"REAL",
	"REAL",
	&asn_OP_REAL,
	asn_DEF_REAL_tags,
	sizeof(asn_DEF_REAL_tags) / sizeof(asn_DEF_REAL_tags[0]),
	asn_DEF_REAL_tags, /* Same as above */
	sizeof(asn_DEF_REAL_tags) / sizeof(asn_DEF_REAL_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0,
	0,	/* No members */
	0	/* No specifics */
};

typedef enum specialRealValue {
	SRV__NOT_A_NUMBER,
	SRV__MINUS_INFINITY,
	SRV__PLUS_INFINITY
} specialRealValue_e;
static struct specialRealValue_s {
	char *string;
	size_t length;
	long dv;
} specialRealValue[] = {
#define	SRV_SET(foo, val)	{ foo, sizeof(foo) - 1, val }
	SRV_SET("<NOT-A-NUMBER/>", 0),
	SRV_SET("<MINUS-INFINITY/>", -1),
	SRV_SET("<PLUS-INFINITY/>", 1),
#undef	SRV_SET
};

ssize_t
REAL__dump(double d, int canonical, asn_app_consume_bytes_f *cb, void *app_key) {
	char local_buf[64];
	char *buf = local_buf;
	ssize_t buflen = sizeof(local_buf);
	const char *fmt = canonical ? "%.17E" /* Precise */ : "%.15f" /* Pleasant*/;
	ssize_t ret;

	/*
	 * Check whether it is a special value.
	 */
	/* fpclassify(3) is not portable yet */
	if(asn_isnan(d)) {
		buf = specialRealValue[SRV__NOT_A_NUMBER].string;
		buflen = specialRealValue[SRV__NOT_A_NUMBER].length;
		return (cb(buf, buflen, app_key) < 0) ? -1 : buflen;
	} else if(!asn_isfinite(d)) {
		if(copysign(1.0, d) < 0.0) {
			buf = specialRealValue[SRV__MINUS_INFINITY].string;
			buflen = specialRealValue[SRV__MINUS_INFINITY].length;
		} else {
			buf = specialRealValue[SRV__PLUS_INFINITY].string;
			buflen = specialRealValue[SRV__PLUS_INFINITY].length;
		}
		return (cb(buf, buflen, app_key) < 0) ? -1 : buflen;
	} else if(ilogb(d) <= -INT_MAX) {
		if(copysign(1.0, d) < 0.0) {
			buf = "-0";
			buflen = 2;
		} else {
			buf = "0";
			buflen = 1;
		}
		return (cb(buf, buflen, app_key) < 0) ? -1 : buflen;
	}

	/*
	 * Use the libc's double printing, hopefully they got it right.
	 */
	do {
		ret = snprintf(buf, buflen, fmt, d);
		if(ret < 0) {
			/* There are some old broken APIs. */
			buflen <<= 1;
			if(buflen > 4096) {
				/* Should be plenty. */
				if(buf != local_buf) FREEMEM(buf);
				return -1;
			}
		} else if(ret >= buflen) {
			buflen = ret + 1;
		} else {
			buflen = ret;
			break;
		}
		if(buf != local_buf) FREEMEM(buf);
		buf = (char *)MALLOC(buflen);
		if(!buf) return -1;
	} while(1);

	if(canonical) {
		/*
		 * Transform the "[-]d.dddE+-dd" output into "[-]d.dddE[-]d"
		 * Check that snprintf() constructed the output correctly.
		 */
		char *dot;
		char *end = buf + buflen;
		char *last_zero;
		char *first_zero_in_run;
        char *s;

        enum {
            LZSTATE_NOTHING,
            LZSTATE_ZEROES
        } lz_state = LZSTATE_NOTHING;

		dot = (buf[0] == 0x2d /* '-' */) ? (buf + 2) : (buf + 1);
		if(*dot >= 0x30) {
			if(buf != local_buf) FREEMEM(buf);
			errno = EINVAL;
			return -1;	/* Not a dot, really */
		}
		*dot = 0x2e;		/* Replace possible comma */

        for(first_zero_in_run = last_zero = s = dot + 2; s < end; s++) {
            switch(*s) {
            case 0x45: /* 'E' */
                if(lz_state == LZSTATE_ZEROES) last_zero = first_zero_in_run;
                break;
            case 0x30: /* '0' */
                if(lz_state == LZSTATE_NOTHING) first_zero_in_run = s;
                lz_state = LZSTATE_ZEROES;
                continue;
            default:
                lz_state = LZSTATE_NOTHING;
                continue;
            }
            break;
        }

		if(s == end) {
			if(buf != local_buf) FREEMEM(buf);
			errno = EINVAL;
			return -1;		/* No promised E */
		}

        assert(*s == 0x45);
        {
            char *E = s;
            char *expptr = ++E;
            char *s = expptr;
            int sign;

            if(*expptr == 0x2b /* '+' */) {
                /* Skip the "+" */
                buflen -= 1;
                sign = 0;
            } else {
                sign = 1;
                s++;
            }
            expptr++;
            if(expptr > end) {
                if(buf != local_buf) FREEMEM(buf);
                errno = EINVAL;
                return -1;
            }
            if(*expptr == 0x30) {
                buflen--;
                expptr++;
            }
            if(lz_state == LZSTATE_ZEROES) {
                *last_zero = 0x45;	/* E */
                buflen -= s - (last_zero + 1);
                s = last_zero + 1;
                if(sign) {
                    *s++ = 0x2d /* '-' */;
                    buflen++;
                }
            }
            for(; expptr <= end; s++, expptr++)
                *s = *expptr;
        }
	} else {
		/*
		 * Remove trailing zeros.
		 */
		char *end = buf + buflen;
		char *last_zero = end;
		int stoplooking = 0;
		char *z;
		for(z = end - 1; z > buf; z--) {
			switch(*z) {
			case 0x30:
				if(!stoplooking)
					last_zero = z;
				continue;
			case 0x31: case 0x32: case 0x33: case 0x34:
			case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
				stoplooking = 1;
				continue;
			default:	/* Catch dot and other separators */
				/*
				 * Replace possible comma (which may even
				 * be not a comma at all: locale-defined).
				 */
				*z = 0x2e;
				if(last_zero == z + 1) {	/* leave x.0 */
					last_zero++;
				}
				buflen = last_zero - buf;
				*last_zero = '\0';
				break;
			}
			break;
		}
	}

	ret = cb(buf, buflen, app_key);
	if(buf != local_buf) FREEMEM(buf);
	return (ret < 0) ? -1 : buflen;
}

int
REAL_print(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
           asn_app_consume_bytes_f *cb, void *app_key) {
    const REAL_t *st = (const REAL_t *)sptr;
	ssize_t ret;
	double d;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(!st || !st->buf)
		ret = cb("<absent>", 8, app_key);
	else if(asn_REAL2double(st, &d))
		ret = cb("<error>", 7, app_key);
	else
		ret = REAL__dump(d, 0, cb, app_key);

	return (ret < 0) ? -1 : 0;
}

int
REAL_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
             const void *bptr) {
    const REAL_t *a = aptr;
    const REAL_t *b = bptr;

    (void)td;

    if(a && b) {
        double adbl, bdbl;
        int ra, rb;
        ra = asn_REAL2double(a, &adbl);
        rb = asn_REAL2double(b, &bdbl);
        if(ra == 0 && rb == 0) {
            if(asn_isnan(adbl)) {
                if(asn_isnan(bdbl)) {
                    return 0;
                } else {
                    return -1;
                }
            } else if(asn_isnan(bdbl)) {
                return 1;
            }
            /* Value comparison. */
            if(adbl < bdbl) {
                return -1;
            } else if(adbl > bdbl) {
                return 1;
            } else {
                return 0;
            }
        } else if(ra) {
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

asn_enc_rval_t
REAL_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
                enum xer_encoder_flags_e flags, asn_app_consume_bytes_f *cb,
                void *app_key) {
    const REAL_t *st = (const REAL_t *)sptr;
	asn_enc_rval_t er;
	double d;

	(void)ilevel;

	if(!st || !st->buf || asn_REAL2double(st, &d))
		ASN__ENCODE_FAILED;

	er.encoded = REAL__dump(d, flags & XER_F_CANONICAL, cb, app_key);
	if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}


/*
 * Decode the chunk of XML text encoding REAL.
 */
static enum xer_pbd_rval
REAL__xer_body_decode(const asn_TYPE_descriptor_t *td, void *sptr,
                      const void *chunk_buf, size_t chunk_size) {
    REAL_t *st = (REAL_t *)sptr;
	double value;
	const char *xerdata = (const char *)chunk_buf;
	char *endptr = 0;
	char *b;

	(void)td;

	if(!chunk_size) return XPBD_BROKEN_ENCODING;

	/*
	 * Decode an XMLSpecialRealValue: <MINUS-INFINITY>, etc.
	 */
	if(xerdata[0] == 0x3c /* '<' */) {
		size_t i;
		for(i = 0; i < sizeof(specialRealValue)
				/ sizeof(specialRealValue[0]); i++) {
			struct specialRealValue_s *srv = &specialRealValue[i];
			double dv;

			if(srv->length != chunk_size
			|| memcmp(srv->string, chunk_buf, chunk_size))
				continue;

			/*
			 * It could've been done using
			 * (double)srv->dv / real_zero,
			 * but it summons fp exception on some platforms.
			 */
			switch(srv->dv) {
			case -1: dv = - INFINITY; break;
			case 0: dv = NAN;	break;
			case 1: dv = INFINITY;	break;
			default: return XPBD_SYSTEM_FAILURE;
			}

			if(asn_double2REAL(st, dv))
				return XPBD_SYSTEM_FAILURE;

			return XPBD_BODY_CONSUMED;
		}
		ASN_DEBUG("Unknown XMLSpecialRealValue");
		return XPBD_BROKEN_ENCODING;
	}

	/*
	 * Copy chunk into the nul-terminated string, and run strtod.
	 */
	b = (char *)MALLOC(chunk_size + 1);
	if(!b) return XPBD_SYSTEM_FAILURE;
	memcpy(b, chunk_buf, chunk_size);
	b[chunk_size] = 0;	/* nul-terminate */

	value = strtod(b, &endptr);
	FREEMEM(b);
	if(endptr == b) return XPBD_BROKEN_ENCODING;

	if(asn_double2REAL(st, value))
		return XPBD_SYSTEM_FAILURE;

	return XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
REAL_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                const asn_TYPE_descriptor_t *td, void **sptr,
                const char *opt_mname, const void *buf_ptr, size_t size) {
    return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(REAL_t), opt_mname,
		buf_ptr, size, REAL__xer_body_decode);
}

int
asn_REAL2double(const REAL_t *st, double *dbl_value) {
	unsigned int octv;

	if(!st || !st->buf) {
		errno = EINVAL;
		return -1;
	}

	if(st->size == 0) {
		*dbl_value = 0;
		return 0;
	}

	octv = st->buf[0];	/* unsigned byte */

	switch(octv & 0xC0) {
	case 0x40:	/* X.690: 8.5.6 a) => 8.5.9 */
		/* "SpecialRealValue" */

		/* Be liberal in what you accept...
		 * http://en.wikipedia.org/wiki/Robustness_principle
		if(st->size != 1) ...
		*/

		switch(st->buf[0]) {
		case 0x40:	/* 01000000: PLUS-INFINITY */
			*dbl_value = INFINITY;
			return 0;
		case 0x41:	/* 01000001: MINUS-INFINITY */
			*dbl_value = - INFINITY;
			return 0;
		case 0x42:	/* 01000010: NOT-A-NUMBER */
			*dbl_value = NAN;
			return 0;
		case 0x43:	/* 01000011: minus zero */
			*dbl_value = -0.0;
			return 0;
		}

		errno = EINVAL;
		return -1;
	case 0x00: {	/* X.690: 8.5.7 */
		/*
		 * Decimal. NR{1,2,3} format from ISO 6093.
		 * NR1: [ ]*[+-]?[0-9]+
		 * NR2: [ ]*[+-]?([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)
		 * NR3: [ ]*[+-]?([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)[Ee][+-]?[0-9]+
		 */
		double d;
		char *source = 0;
		char *endptr;
		int used_malloc = 0;

		if(octv == 0 || (octv & 0x3C)) {
			/* Remaining values of bits 6 to 1 are Reserved. */
			errno = EINVAL;
			return -1;
		}

		/* 1. By contract, an input buffer should be '\0'-terminated.
		 * OCTET STRING decoder ensures that, as is asn_double2REAL().
		 * 2. ISO 6093 specifies COMMA as a possible decimal separator.
		 * However, strtod() can't always deal with COMMA.
		 * So her we fix both by reallocating, copying and fixing.
		 */
		if(st->buf[st->size] != '\0' || memchr(st->buf, ',', st->size)) {
			const uint8_t *p, *end;
			char *b;

            b = source = (char *)MALLOC(st->size + 1);
            if(!source) return -1;
            used_malloc = 1;

			/* Copy without the first byte and with 0-termination */
			for(p = st->buf + 1, end = st->buf + st->size;
					p < end; b++, p++)
				*b = (*p == ',') ? '.' : *p;
			*b = '\0';
		} else {
			source = (char *)&st->buf[1];
		}

		endptr = source;
		d = strtod(source, &endptr);
		if(*endptr != '\0') {
			/* Format is not consistent with ISO 6093 */
			if(used_malloc) FREEMEM(source);
			errno = EINVAL;
			return -1;
		}
		if(used_malloc) FREEMEM(source);
		if(asn_isfinite(d)) {
			*dbl_value = d;
			return 0;
		} else {
			errno = ERANGE;
			return -1;
		}
	  }
	}

	/*
	 * Binary representation.
	 */
    {
	double m;
	int32_t expval;		/* exponent value */
	unsigned int elen;	/* exponent value length, in octets */
	int scaleF;
	int baseF;
	uint8_t *ptr;
	uint8_t *end;
	int sign;

	switch((octv & 0x30) >> 4) {
	case 0x00: baseF = 1; break;	/* base 2 */
	case 0x01: baseF = 3; break;	/* base 8 */
	case 0x02: baseF = 4; break;	/* base 16 */
	default:
		/* Reserved field, can't parse now. */
		errno = EINVAL;
		return -1;
	}

	sign = (octv & 0x40);	/* bit 7 */
	scaleF = (octv & 0x0C) >> 2;	/* bits 4 to 3 */

	if(st->size <= 1 + (octv & 0x03)) {
		errno = EINVAL;
		return -1;
	}

	elen = (octv & 0x03);	/* bits 2 to 1; 8.5.6.4 */
	if(elen == 0x03) {	/* bits 2 to 1 = 11; 8.5.6.4, case d) */
		elen = st->buf[1];	/* unsigned binary number */
		if(elen == 0 || st->size <= (2 + elen)) {
			errno = EINVAL;
			return -1;
		}
		/* FIXME: verify constraints of case d) */
		ptr = &st->buf[2];
	} else {
		ptr = &st->buf[1];
	}

	/* Fetch the multibyte exponent */
	expval = (int)(*(int8_t *)ptr);
	if(elen >= sizeof(expval)-1) {
		errno = ERANGE;
		return -1;
	}
	end = ptr + elen + 1;
	for(ptr++; ptr < end; ptr++)
		expval = (expval * 256) + *ptr;

	m = 0.0;	/* Initial mantissa value */

	/* Okay, the exponent is here. Now, what about mantissa? */
	end = st->buf + st->size;
	for(; ptr < end; ptr++)
		m = ldexp(m, 8) + *ptr;

	if(0)
	ASN_DEBUG("m=%.10f, scF=%d, bF=%d, expval=%d, ldexp()=%f, ldexp()=%f\n",
		m, scaleF, baseF, expval,
		ldexp(m, expval * baseF + scaleF),
		ldexp(m, scaleF) * pow(pow(2, baseF), expval)
	);

	/*
	 * (S * N * 2^F) * B^E
	 * Essentially:
	m = ldexp(m, scaleF) * pow(pow(2, baseF), expval);
	 */
	m = ldexp(m, expval * baseF + scaleF);
	if(asn_isfinite(m)) {
		*dbl_value = sign ? -m : m;
	} else {
		errno = ERANGE;
		return -1;
	}

    } /* if(binary_format) */

	return 0;
}

/*
 * Assume IEEE 754 floating point: standard 64 bit double.
 * [1 bit sign]  [11 bits exponent]  [52 bits mantissa]
 */
int
asn_double2REAL(REAL_t *st, double dbl_value) {
    double test = -0.0;
    int float_big_endian = *(const char *)&test != 0;
	uint8_t buf[16];	/* More than enough for 8-byte dbl_value */
	uint8_t dscr[sizeof(dbl_value)];	/* double value scratch pad */
	/* Assertion guards: won't even compile, if unexpected double size */
	char assertion_buffer1[9 - sizeof(dbl_value)] CC_NOTUSED;
	char assertion_buffer2[sizeof(dbl_value) - 7] CC_NOTUSED;
	uint8_t *ptr = buf;
	uint8_t *mstop;		/* Last byte of mantissa */
	unsigned int mval;	/* Value of the last byte of mantissa */
	unsigned int bmsign;	/* binary mask with sign */
	unsigned int buflen;
	unsigned int accum;
	int expval;

	if(!st) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * ilogb(+-0) returns -INT_MAX or INT_MIN (platform-dependent)
	 * ilogb(+-inf) returns INT_MAX, logb(+-inf) returns +inf
	 * ilogb(NaN) returns INT_MIN or INT_MAX (platform-dependent)
	 */
	expval = ilogb(dbl_value);
	if(expval <= -INT_MAX	/* Also catches +-0 and maybe isnan() */
	|| expval == INT_MAX	/* catches isfin() and maybe isnan() */
	) {
		if(!st->buf || st->size < 2) {
			ptr = (uint8_t *)MALLOC(2);
			if(!ptr) return -1;
			if(st->buf) FREEMEM(st->buf);
			st->buf = ptr;
		}
		/* fpclassify(3) is not portable yet */
		if(asn_isnan(dbl_value)) {
			st->buf[0] = 0x42;	/* NaN */
			st->buf[1] = 0;
			st->size = 1;
		} else if(!asn_isfinite(dbl_value)) {
			if(copysign(1.0, dbl_value) < 0.0) {
				st->buf[0] = 0x41;	/* MINUS-INFINITY */
			} else {
				st->buf[0] = 0x40;	/* PLUS-INFINITY */
			}
			st->buf[1] = 0;
			st->size = 1;
		} else {
			if(copysign(1.0, dbl_value) >= 0.0) {
				/* no content octets: positive zero */
				st->buf[0] = 0;	/* JIC */
				st->size = 0;
			} else {
				/* Negative zero. #8.5.3, 8.5.9 */
				st->buf[0] = 0x43;
				st->buf[1] = 0;
				st->size = 1;
			}
		}
		return 0;
	}

	if(float_big_endian) {
		uint8_t *s = ((uint8_t *)&dbl_value) + 1;
		uint8_t *end = ((uint8_t *)&dbl_value) + sizeof(double);
		uint8_t *d;

		bmsign = 0x80 | ((s[-1] >> 1) & 0x40);	/* binary mask & - */
		for(mstop = d = dscr; s < end; d++, s++) {
			*d = *s;
			if(*d) mstop = d;
		}
    } else {
		uint8_t *s = ((uint8_t *)&dbl_value) + sizeof(dbl_value) - 2;
		uint8_t *start = ((uint8_t *)&dbl_value);
		uint8_t *d;

		bmsign = 0x80 | ((s[1] >> 1) & 0x40);	/* binary mask & - */
		for(mstop = d = dscr; s >= start; d++, s--) {
			*d = *s;
			if(*d) mstop = d;
		}
    }

	/* Remove parts of the exponent, leave mantissa and explicit 1. */
	dscr[0] = 0x10 | (dscr[0] & 0x0f);

	/* Adjust exponent in a very unobvious way */
	expval -= 8 * ((mstop - dscr) + 1) - 4;

	/* This loop ensures DER conformance by forcing mantissa odd: 11.3.1 */
	mval = *mstop;
	if(mval && !(mval & 1)) {
		int shift_count = 1;
		int ishift;
		uint8_t *mptr;

		/*
		 * Figure out what needs to be done to make mantissa odd.
		 */
		if(!(mval & 0x0f))	/* Speed-up a little */
			shift_count = 4;
		while(((mval >> shift_count) & 1) == 0)
			shift_count++;

		ishift = 8 - shift_count;
		accum = 0;

		/* Go over the buffer, shifting it shift_count bits right. */
		for(mptr = dscr; mptr <= mstop; mptr++) {
			mval = *mptr;
			*mptr = accum | (mval >> shift_count);
			accum = mval << ishift;
		}

		/* Adjust exponent appropriately. */
		expval += shift_count;
	}

	if(expval < 0) {
		if((expval >> 7) == -1) {
			*ptr++ = bmsign | 0x00;
			*ptr++ = expval;
		} else if((expval >> 15) == -1) {
			*ptr++ = bmsign | 0x01;
			*ptr++ = expval >> 8;
			*ptr++ = expval;
		} else {
			*ptr++ = bmsign | 0x02;
			*ptr++ = expval >> 16;
			*ptr++ = expval >> 8;
			*ptr++ = expval;
		}
	} else if(expval <= 0x7f) {
		*ptr++ = bmsign | 0x00;
		*ptr++ = expval;
	} else if(expval <= 0x7fff) {
		*ptr++ = bmsign | 0x01;
		*ptr++ = expval >> 8;
		*ptr++ = expval;
	} else {
		assert(expval <= 0x7fffff);
		*ptr++ = bmsign | 0x02;
		*ptr++ = expval >> 16;
		*ptr++ = expval >> 8;
		*ptr++ = expval;
	}

	buflen = (mstop - dscr) + 1;
	memcpy(ptr, dscr, buflen);
	ptr += buflen;
	buflen = ptr - buf;

	ptr = (uint8_t *)MALLOC(buflen + 1);
	if(!ptr) return -1;

	memcpy(ptr, buf, buflen);
	buf[buflen] = 0;	/* JIC */

	if(st->buf) FREEMEM(st->buf);
	st->buf = ptr;
	st->size = buflen;

	return 0;
}

int CC_ATTR_NO_SANITIZE("float-cast-overflow")
asn_double2float(double d, float *outcome) {
    float f = d;

    *outcome = f;

    if(asn_isfinite(d) == asn_isfinite(f)) {
        return 0;
    } else {
        return -1;
    }
}

#ifndef ASN_DISABLE_OER_SUPPORT

/*
 * Encode as Canonical OER
 */
asn_enc_rval_t
REAL_encode_oer(const asn_TYPE_descriptor_t *td,
                const asn_oer_constraints_t *constraints, const void *sptr,
                asn_app_consume_bytes_f *cb, void *app_key) {
    const REAL_t *st = sptr;
    asn_enc_rval_t er;
    ssize_t len_len;

    if(!st || !st->buf || !td)
        ASN__ENCODE_FAILED;

    if(!constraints) constraints = td->encoding_constraints.oer_constraints;
    if(constraints && constraints->value.width != 0) {
        /* If we're constrained to a narrow float/double representation, we
         * shouldn't have ended up using REAL. Expecting NativeReal. */
        ASN__ENCODE_FAILED;
    }

    /* Encode a fake REAL */
    len_len = oer_serialize_length(st->size, cb, app_key);
    if(len_len < 0 || cb(st->buf, st->size, app_key) < 0) {
        ASN__ENCODE_FAILED;
    } else {
        er.encoded = len_len + st->size;
        ASN__ENCODED_OK(er);
    }
}

asn_dec_rval_t
REAL_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                const asn_TYPE_descriptor_t *td,
                const asn_oer_constraints_t *constraints, void **sptr,
                const void *ptr, size_t size) {
    asn_dec_rval_t ok = {RC_OK, 0};
    REAL_t *st;
    uint8_t *buf;
    ssize_t len_len;
    size_t real_body_len;

    (void)opt_codec_ctx;

    if(!constraints) constraints = td->encoding_constraints.oer_constraints;
    if(constraints && constraints->value.width != 0) {
        /* If we're constrained to a narrow float/double representation, we
         * shouldn't have ended up using REAL. Expecting NativeReal. */
        ASN__DECODE_FAILED;
    }

    len_len = oer_fetch_length(ptr, size, &real_body_len);
    if(len_len < 0) ASN__DECODE_FAILED;
    if(len_len == 0) ASN__DECODE_STARVED;

    ptr = (const char *)ptr + len_len;
    size -= len_len;

    if(real_body_len > size) ASN__DECODE_STARVED;

    buf = CALLOC(1, real_body_len + 1);
    if(!buf) ASN__DECODE_FAILED;

    if(!(st = *sptr)) {
        st = (*sptr = CALLOC(1, sizeof(REAL_t)));
        if(!st) {
            FREEMEM(buf);
            ASN__DECODE_FAILED;
        }
    } else {
        FREEMEM(st->buf);
    }

    memcpy(buf, ptr, real_body_len);
    buf[real_body_len] = '\0';

    st->buf = buf;
    st->size = real_body_len;

    ok.consumed = len_len + real_body_len;
    return ok;
}

#endif  /* ASN_DISABLE_OER_SUPPORT */

#ifndef ASN_DISABLE_PER_SUPPORT

asn_dec_rval_t
REAL_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                 const asn_TYPE_descriptor_t *td,
                 const asn_per_constraints_t *constraints, void **sptr,
                 asn_per_data_t *pd) {
    (void)constraints;	/* No PER visible constraints */
	return OCTET_STRING_decode_uper(opt_codec_ctx, td, 0, sptr, pd);
}

asn_enc_rval_t
REAL_encode_uper(const asn_TYPE_descriptor_t *td,
                 const asn_per_constraints_t *constraints, const void *sptr,
                 asn_per_outp_t *po) {
    (void)constraints;	/* No PER visible constraints */
	return OCTET_STRING_encode_uper(td, 0, sptr, po);
}

#endif  /* ASN_DISABLE_PER_SUPPORT */

asn_random_fill_result_t
REAL_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                       const asn_encoding_constraints_t *constraints,
                       size_t max_length) {
    asn_random_fill_result_t result_ok = {ARFILL_OK, 1};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    static const double values[] = {
        0, -0.0, -1, 1, -M_E, M_E, -3.14, 3.14, -M_PI, M_PI, -255, 255,
        /* 2^51 */
        -2251799813685248.0, 2251799813685248.0,
        /* 2^52 */
        -4503599627370496.0, 4503599627370496.0,
        /* 2^100 */
        -1267650600228229401496703205376.0, 1267650600228229401496703205376.0,
        -FLT_MIN, FLT_MIN,
        -FLT_MAX, FLT_MAX,
        -DBL_MIN, DBL_MIN,
        -DBL_MAX, DBL_MAX,
#ifdef  FLT_TRUE_MIN
        -FLT_TRUE_MIN, FLT_TRUE_MIN,
#endif
#ifdef  DBL_TRUE_MIN
        -DBL_TRUE_MIN, DBL_TRUE_MIN,
#endif
        INFINITY, -INFINITY, NAN};
    REAL_t *st;
    double d;

    (void)constraints;

    if(max_length == 0) return result_skipped;

    d = values[asn_random_between(0, sizeof(values) / sizeof(values[0]) - 1)];

    if(*sptr) {
        st = *sptr;
    } else {
        st = (REAL_t*)(*sptr = CALLOC(1, sizeof(REAL_t)));
        if(!st) {
            return result_failed;
        }
    }

    if(asn_double2REAL(st, d)) {
        if(st == *sptr) {
            ASN_STRUCT_RESET(*td, st);
        } else {
            ASN_STRUCT_FREE(*td, st);
        }
        return result_failed;
    }

    result_ok.length = st->size;
    return result_ok;
}

