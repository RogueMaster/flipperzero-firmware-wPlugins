/*-
 * Copyright (c) 2004-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * Read the NativeReal.h for the explanation wrt. differences between
 * REAL and NativeReal.
 * Basically, both are decoders and encoders of ASN.1 REAL type, but this
 * implementation deals with the standard (machine-specific) representation
 * of them instead of using the platform-independent buffer.
 */
#include <asn_internal.h>
#include <NativeReal.h>
#include <REAL.h>
#include <OCTET_STRING.h>
#include <math.h>
#include <float.h>

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
#pragma clang diagnostic pop
#else
#define asn_isnan(v)    isnan(v)
#endif  /* generic selections */

/*
 * NativeReal basic type description.
 */
static const ber_tlv_tag_t asn_DEF_NativeReal_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (9 << 2))
};
asn_TYPE_operation_t asn_OP_NativeReal = {
	NativeReal_free,
	NativeReal_print,
	NativeReal_compare,
	NativeReal_decode_ber,
	NativeReal_encode_der,
	NativeReal_decode_xer,
	NativeReal_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	NativeReal_decode_oer,
	NativeReal_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	NativeReal_decode_uper,
	NativeReal_encode_uper,
#endif	/* ASN_DISABLE_PER_SUPPORT */
	NativeReal_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_NativeReal = {
	"REAL",			/* The ASN.1 type is still REAL */
	"REAL",
	&asn_OP_NativeReal,
	asn_DEF_NativeReal_tags,
	sizeof(asn_DEF_NativeReal_tags) / sizeof(asn_DEF_NativeReal_tags[0]),
	asn_DEF_NativeReal_tags,	/* Same as above */
	sizeof(asn_DEF_NativeReal_tags) / sizeof(asn_DEF_NativeReal_tags[0]),
	{ 0, 0, asn_generic_no_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

static size_t NativeReal__float_size(const asn_TYPE_descriptor_t *td);
static double NativeReal__get_double(const asn_TYPE_descriptor_t *td,
                                     const void *ptr);
static ssize_t NativeReal__set(const asn_TYPE_descriptor_t *td, void **sptr,
                               double d);

/*
 * Decode REAL type.
 */
asn_dec_rval_t
NativeReal_decode_ber(const asn_codec_ctx_t *opt_codec_ctx,
                      const asn_TYPE_descriptor_t *td, void **sptr,
                      const void *buf_ptr, size_t size, int tag_mode) {
    asn_dec_rval_t rval;
    ber_tlv_len_t length;

    ASN_DEBUG("Decoding %s as REAL (tm=%d)", td->name, tag_mode);

    /*
     * Check tags.
     */
    rval = ber_check_tags(opt_codec_ctx, td, 0, buf_ptr, size, tag_mode, 0,
                          &length, 0);
    if(rval.code != RC_OK) return rval;
    assert(length >= 0);    /* Ensured by ber_check_tags */

    ASN_DEBUG("%s length is %d bytes", td->name, (int)length);

    /*
     * Make sure we have this length.
     */
    buf_ptr = ((const char *)buf_ptr) + rval.consumed;
    size -= rval.consumed;
    if(length > (ber_tlv_len_t)size) {
        rval.code = RC_WMORE;
        rval.consumed = 0;
        return rval;
    }

    /*
     * ASN.1 encoded REAL: buf_ptr, length
     * Fill the Dbl, at the same time checking for overflow.
     * If overflow occurred, return with RC_FAIL.
     */
    {
        uint8_t scratch[24]; /* Longer than %.16f in decimal */
        REAL_t tmp;
        double d;
        int ret;

        if((size_t)length < sizeof(scratch)) {
            tmp.buf = scratch;
            tmp.size = length;
        } else {
            /* This rarely happens: impractically long value */
            tmp.buf = CALLOC(1, length + 1);
            tmp.size = length;
            if(!tmp.buf) {
                rval.code = RC_FAIL;
                rval.consumed = 0;
                return rval;
            }
        }

        memcpy(tmp.buf, buf_ptr, length);
        tmp.buf[length] = '\0';

        ret = asn_REAL2double(&tmp, &d);
        if(tmp.buf != scratch) FREEMEM(tmp.buf);
        if(ret) {
            rval.code = RC_FAIL;
            rval.consumed = 0;
            return rval;
        }

        if(NativeReal__set(td, sptr, d) < 0)
            ASN__DECODE_FAILED;
    }

    rval.code = RC_OK;
    rval.consumed += length;

    ASN_DEBUG("Took %ld/%ld bytes to encode %s", (long)rval.consumed,
              (long)length, td->name);

    return rval;
}

/*
 * Encode the NativeReal using the standard REAL type DER encoder.
 */
asn_enc_rval_t
NativeReal_encode_der(const asn_TYPE_descriptor_t *td, const void *sptr,
                      int tag_mode, ber_tlv_tag_t tag,
                      asn_app_consume_bytes_f *cb, void *app_key) {
    double d = NativeReal__get_double(td, sptr);
    asn_enc_rval_t erval;
	REAL_t tmp;

	/* Prepare a temporary clean structure */
	memset(&tmp, 0, sizeof(tmp));

    if(asn_double2REAL(&tmp, d))
        ASN__ENCODE_FAILED;

    /* Encode a fake REAL */
    erval = der_encode_primitive(td, &tmp, tag_mode, tag, cb, app_key);
    if(erval.encoded == -1) {
		assert(erval.structure_ptr == &tmp);
		erval.structure_ptr = sptr;
	}

	/* Free possibly allocated members of the temporary structure */
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);

    return erval;
}

#ifndef ASN_DISABLE_PER_SUPPORT

/*
 * Decode REAL type using PER.
 */
asn_dec_rval_t
NativeReal_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                       const asn_TYPE_descriptor_t *td,
                       const asn_per_constraints_t *constraints, void **sptr,
                       asn_per_data_t *pd) {
    asn_dec_rval_t rval;
    double d;
	REAL_t tmp;
	void *ptmp = &tmp;
	int ret;

	(void)constraints;

	memset(&tmp, 0, sizeof(tmp));
    rval = OCTET_STRING_decode_uper(opt_codec_ctx, &asn_DEF_REAL,
                                    NULL, &ptmp, pd);
    if(rval.code != RC_OK) {
		ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);
		return rval;
	}

	ret = asn_REAL2double(&tmp, &d);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);
	if(ret) ASN__DECODE_FAILED;

    if(NativeReal__set(td, sptr, d) < 0 )
        ASN__DECODE_FAILED;

	return rval;
}

/*
 * Encode the NativeReal using the OCTET STRING PER encoder.
 */
asn_enc_rval_t
NativeReal_encode_uper(const asn_TYPE_descriptor_t *td,
                       const asn_per_constraints_t *constraints,
                       const void *sptr, asn_per_outp_t *po) {
    double d = NativeReal__get_double(td, sptr);
	asn_enc_rval_t erval;
	REAL_t tmp;

	(void)constraints;

	/* Prepare a temporary clean structure */
	memset(&tmp, 0, sizeof(tmp));

	if(asn_double2REAL(&tmp, d))
		ASN__ENCODE_FAILED;
	
	/* Encode a DER REAL */
    erval = OCTET_STRING_encode_uper(&asn_DEF_REAL, NULL, &tmp, po);
    if(erval.encoded == -1)
		erval.structure_ptr = sptr;

	/* Free possibly allocated members of the temporary structure */
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);

	return erval;
}

#endif /* ASN_DISABLE_PER_SUPPORT */

#ifndef ASN_DISABLE_OER_SUPPORT

/*
 * Swap bytes from/to network, if local is little-endian.
 * Unused endianness sections are likely removed at compile phase.
 */
static void
NativeReal__network_swap(size_t float_size, const void *srcp, uint8_t *dst) {
    const uint8_t *src = srcp;
    double test = -0.0;
    int float_big_endian = *(const char *)&test != 0;
    /* In lieu of static_assert(sizeof(double) == 8) */
    static const char sizeof_double_is_8_a[sizeof(double)-7] CC_NOTUSED;
    static const char sizeof_double_is_8_b[9-sizeof(double)] CC_NOTUSED;
    /* In lieu of static_assert(sizeof(sizeof) == 4) */
    static const char sizeof_float_is_4_a[sizeof(float)-3] CC_NOTUSED;
    static const char sizeof_float_is_4_b[5-sizeof(float)] CC_NOTUSED;

    switch(float_size) {
    case sizeof(double):
        assert(sizeof(double) == 8);
        if(float_big_endian) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
            dst[4] = src[4];
            dst[5] = src[5];
            dst[6] = src[6];
            dst[7] = src[7];
        } else {
            dst[0] = src[7];
            dst[1] = src[6];
            dst[2] = src[5];
            dst[3] = src[4];
            dst[4] = src[3];
            dst[5] = src[2];
            dst[6] = src[1];
            dst[7] = src[0];
        }
        return;
    case sizeof(float):
        assert(sizeof(float) == 4);
        if(float_big_endian) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        } else {
            dst[0] = src[3];
            dst[1] = src[2];
            dst[2] = src[1];
            dst[3] = src[0];
        }
        return;
    }
}

/*
 * Encode as Canonical OER.
 */
asn_enc_rval_t
NativeReal_encode_oer(const asn_TYPE_descriptor_t *td,
                      const asn_oer_constraints_t *constraints,
                      const void *sptr, asn_app_consume_bytes_f *cb,
                      void *app_key) {
    asn_enc_rval_t er = {0, 0, 0};

    if(!constraints) constraints = td->encoding_constraints.oer_constraints;
    if(constraints && constraints->value.width != 0) {
        /* X.696 IEEE 754 binary32 and binary64 encoding */
        uint8_t scratch[sizeof(double)];
        const asn_NativeReal_specifics_t *specs =
            (const asn_NativeReal_specifics_t *)td->specifics;
        size_t wire_size = constraints->value.width;

        if(specs ? (wire_size == specs->float_size)
                 : (wire_size == sizeof(double))) {
            /*
             * Our representation matches the wire, modulo endianness.
             * That was the whole point of compact encoding!
             */
        } else {
            assert((wire_size == sizeof(double))
                   || (specs && specs->float_size == wire_size));
            ASN__ENCODE_FAILED;
        }

        /*
         * The X.696 standard doesn't specify endianness, neither is IEEE 754.
         * So we assume the network format is big endian.
         */
        NativeReal__network_swap(wire_size, sptr, scratch);
        if(cb(scratch, wire_size, app_key) < 0) {
            ASN__ENCODE_FAILED;
        } else {
            er.encoded = wire_size;
            ASN__ENCODED_OK(er);
        }
    } else {
        double d = NativeReal__get_double(td, sptr);
        ssize_t len_len;
        REAL_t tmp;

        /* Prepare a temporary clean structure */
        memset(&tmp, 0, sizeof(tmp));

        if(asn_double2REAL(&tmp, d)) {
            ASN__ENCODE_FAILED;
        }

        /* Encode a fake REAL */
        len_len = oer_serialize_length(tmp.size, cb, app_key);
        if(len_len < 0 || cb(tmp.buf, tmp.size, app_key) < 0) {
            ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);
            ASN__ENCODE_FAILED;
        } else {
            er.encoded = len_len + tmp.size;
            ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &tmp);
            ASN__ENCODED_OK(er);
        }
    }
}

asn_dec_rval_t
NativeReal_decode_oer(const asn_codec_ctx_t *opt_codec_ctx,
                      const asn_TYPE_descriptor_t *td,
                      const asn_oer_constraints_t *constraints, void **sptr,
                      const void *ptr, size_t size) {
    asn_dec_rval_t ok = {RC_OK, 0};
    double d;
    ssize_t len_len;
    size_t real_body_len;

    (void)opt_codec_ctx;

    if(!constraints) constraints = td->encoding_constraints.oer_constraints;
    if(constraints && constraints->value.width != 0) {
        /* X.696 IEEE 754 binary32 and binary64 encoding */
        uint8_t scratch[sizeof(double)];
        size_t wire_size = constraints->value.width;

        if(size < wire_size)
            ASN__DECODE_STARVED;

        /*
         * The X.696 standard doesn't specify endianness, neither is IEEE 754.
         * So we assume the network format is big endian.
         */
        NativeReal__network_swap(wire_size, ptr, scratch);


        switch(wire_size) {
            case sizeof(double):
                {
                    double tmp;
                    memcpy(&tmp, scratch, sizeof(double));
                    if(NativeReal__set(td, sptr, tmp) < 0)
                        ASN__DECODE_FAILED;
                }
                break;
            case sizeof(float):
                {
                    float tmp;
                    memcpy(&tmp, scratch, sizeof(float));
                    if(NativeReal__set(td, sptr, tmp) < 0)
                        ASN__DECODE_FAILED;
                }
                break;
        default:
            ASN__DECODE_FAILED;
        }

        ok.consumed = wire_size;
        return ok;
    }

    len_len = oer_fetch_length(ptr, size, &real_body_len);
    if(len_len < 0) ASN__DECODE_FAILED;
    if(len_len == 0) ASN__DECODE_STARVED;

    ptr = (const char *)ptr + len_len;
    size -= len_len;

    if(real_body_len > size) ASN__DECODE_STARVED;

    {
        uint8_t scratch[24]; /* Longer than %.16f in decimal */
        REAL_t tmp;
        int ret;

        if(real_body_len < sizeof(scratch)) {
            tmp.buf = scratch;
            tmp.size = real_body_len;
        } else {
            /* This rarely happens: impractically long value */
            tmp.buf = CALLOC(1, real_body_len + 1);
            tmp.size = real_body_len;
            if(!tmp.buf) {
                ASN__DECODE_FAILED;
            }
        }

        memcpy(tmp.buf, ptr, real_body_len);
        tmp.buf[real_body_len] = '\0';

        ret = asn_REAL2double(&tmp, &d);
        if(tmp.buf != scratch) FREEMEM(tmp.buf);
        if(ret) {
            ASN_DEBUG("REAL decoded in %" ASN_PRI_SIZE " bytes, but can't convert t double",
                      real_body_len);
            ASN__DECODE_FAILED;
        }
    }

    if(NativeReal__set(td, sptr, d) < 0)
        ASN__DECODE_FAILED;

    ok.consumed = len_len + real_body_len;
    return ok;
}

#endif /* ASN_DISABLE_OER_SUPPORT */

/*
 * Decode the chunk of XML text encoding REAL.
 */
asn_dec_rval_t
NativeReal_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                      const asn_TYPE_descriptor_t *td, void **sptr,
                      const char *opt_mname, const void *buf_ptr, size_t size) {
    asn_dec_rval_t rval;
	REAL_t st = { 0, 0 };
	REAL_t *stp = &st;

	rval = REAL_decode_xer(opt_codec_ctx, td, (void **)&stp, opt_mname,
		buf_ptr, size);
	if(rval.code == RC_OK) {
        double d;
        if(asn_REAL2double(&st, &d) || NativeReal__set(td, sptr, d) < 0) {
            rval.code = RC_FAIL;
            rval.consumed = 0;
        }
	} else {
        /* Convert all errors into RC_FAIL */
        rval.consumed = 0;
	}
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_REAL, &st);
	return rval;
}

asn_enc_rval_t
NativeReal_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
                      int ilevel, enum xer_encoder_flags_e flags,
                      asn_app_consume_bytes_f *cb, void *app_key) {
    double d = NativeReal__get_double(td, sptr);
	asn_enc_rval_t er;

	(void)ilevel;

    er.encoded = REAL__dump(d, flags & XER_F_CANONICAL, cb, app_key);
    if(er.encoded < 0) ASN__ENCODE_FAILED;

	ASN__ENCODED_OK(er);
}

/*
 * REAL specific human-readable output.
 */
int
NativeReal_print(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
                 asn_app_consume_bytes_f *cb, void *app_key) {
    (void)ilevel;	/* Unused argument */

	if(sptr) {
        double d = NativeReal__get_double(td, sptr);
        return (REAL__dump(d, 0, cb, app_key) < 0) ? -1 : 0;
    } else {
        return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;
    }
}

int
NativeReal_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
                   const void *bptr) {

    if(aptr && bptr) {
        double a = NativeReal__get_double(td, aptr);
        double b = NativeReal__get_double(td, bptr);

        /* NaN sorted above everything else */
        if(asn_isnan(a)) {
            if(asn_isnan(b)) {
                return 0;
            } else {
                return -1;
            }
        } else if(asn_isnan(b)) {
            return 1;
        }
        /* Value comparison. */
        if(a < b) {
            return -1;
        } else if(a > b) {
            return 1;
        } else {
            return 0;
        }
    } else if(!aptr) {
        return -1;
    } else {
        return 1;
    }
}

void
NativeReal_free(const asn_TYPE_descriptor_t *td, void *ptr,
                enum asn_struct_free_method method) {
    if(!td || !ptr)
		return;

	ASN_DEBUG("Freeing %s as REAL (%d, %p, Native)",
		td->name, method, ptr);

    switch(method) {
    case ASFM_FREE_EVERYTHING:
        FREEMEM(ptr);
        break;
    case ASFM_FREE_UNDERLYING:
        break;
    case ASFM_FREE_UNDERLYING_AND_RESET: {
        const asn_NativeReal_specifics_t *specs;
        size_t float_size;
        specs = (const asn_NativeReal_specifics_t *)td->specifics;
        float_size = specs ? specs->float_size : sizeof(double);
        memset(ptr, 0, float_size);
    } break;
    }
}

asn_random_fill_result_t
NativeReal_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                       const asn_encoding_constraints_t *constraints,
                       size_t max_length) {
    asn_random_fill_result_t result_ok = {ARFILL_OK, 0};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif
#ifndef NAN
#define NAN (0.0/0.0)
#endif
    static const double double_values[] = {
        -M_E, M_E, -M_PI, M_PI, /* Better precision than with floats */
        -1E+308, 1E+308,
        /* 2^51 */
        -2251799813685248.0, 2251799813685248.0,
        /* 2^52 */
        -4503599627370496.0, 4503599627370496.0,
        /* 2^100 */
        -1267650600228229401496703205376.0, 1267650600228229401496703205376.0,
        -DBL_MIN, DBL_MIN,
        -DBL_MAX, DBL_MAX,
#ifdef  DBL_TRUE_MIN
        -DBL_TRUE_MIN, DBL_TRUE_MIN
#endif
    };
    static const float float_values[] = {
        0, -0.0, -1, 1, -M_E, M_E, -3.14, 3.14, -M_PI, M_PI, -255, 255,
        -FLT_MIN, FLT_MIN,
        -FLT_MAX, FLT_MAX,
#ifdef  FLT_TRUE_MIN
        -FLT_TRUE_MIN, FLT_TRUE_MIN,
#endif
        INFINITY, -INFINITY, NAN
    };
    ssize_t float_set_size = NativeReal__float_size(td);
    const size_t n_doubles = sizeof(double_values) / sizeof(double_values[0]);
    const size_t n_floats = sizeof(float_values) / sizeof(float_values[0]);
    double d;

    (void)constraints;

    if(max_length == 0) return result_skipped;

    if(float_set_size == sizeof(double) && asn_random_between(0, 1) == 0) {
        d = double_values[asn_random_between(0, n_doubles - 1)];
    } else {
        d = float_values[asn_random_between(0, n_floats - 1)];
    }

    if(NativeReal__set(td, sptr, d) < 0) {
        return result_failed;
    }

    result_ok.length = float_set_size;
    return result_ok;
}


/*
 * Local helper functions.
 */

static size_t
NativeReal__float_size(const asn_TYPE_descriptor_t *td) {
    const asn_NativeReal_specifics_t *specs =
        (const asn_NativeReal_specifics_t *)td->specifics;
    return specs ? specs->float_size : sizeof(double);
}

static double
NativeReal__get_double(const asn_TYPE_descriptor_t *td, const void *ptr) {
    size_t float_size = NativeReal__float_size(td);
    if(float_size == sizeof(float)) {
        return *(const float *)ptr;
    } else {
        return *(const double *)ptr;
    }
}

static ssize_t  /* Returns -1 or float size. */
NativeReal__set(const asn_TYPE_descriptor_t *td, void **sptr, double d) {
    size_t float_size = NativeReal__float_size(td);
    void *native;

    if(!(native = *sptr)) {
        native = (*sptr = CALLOC(1, float_size));
        if(!native) {
            return -1;
        }
    }

    if(float_size == sizeof(float)) {
        if(asn_double2float(d, (float *)native)) {
            return -1;
        }
    } else {
        *(double *)native = d;
    }

    return float_size;
}

