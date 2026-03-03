/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_GeneralizedTime_H_
#define	_GeneralizedTime_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t GeneralizedTime_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_GeneralizedTime;
extern asn_TYPE_operation_t asn_OP_GeneralizedTime;

asn_struct_print_f GeneralizedTime_print;
asn_struct_compare_f GeneralizedTime_compare;
asn_constr_check_f GeneralizedTime_constraint;
der_type_encoder_f GeneralizedTime_encode_der;
xer_type_encoder_f GeneralizedTime_encode_xer;
asn_random_fill_f  GeneralizedTime_random_fill;

#define GeneralizedTime_free           OCTET_STRING_free
#define GeneralizedTime_decode_ber     OCTET_STRING_decode_ber
#define GeneralizedTime_decode_xer     OCTET_STRING_decode_xer_utf8
#define GeneralizedTime_decode_uper    OCTET_STRING_decode_uper
#define GeneralizedTime_encode_uper    OCTET_STRING_encode_uper

/***********************
 * Some handy helpers. *
 ***********************/

struct tm;	/* <time.h> */

/*
 * Convert a GeneralizedTime structure into time_t
 * and optionally into struct tm.
 * If as_gmt is given, the resulting _optional_tm4fill will have a GMT zone,
 * instead of default local one.
 * On error returns -1 and errno set to EINVAL
 */
time_t asn_GT2time(const GeneralizedTime_t *, struct tm *_optional_tm4fill,
	int as_gmt);

/* A version of the above function also returning the fractions of seconds */
time_t asn_GT2time_frac(const GeneralizedTime_t *,
	int *frac_value, int *frac_digits,	/* (value / (10 ^ digits)) */
	struct tm *_optional_tm4fill, int as_gmt);

/*
 * Another version returning fractions with defined precision
 * For example, parsing of the time ending with ".1" seconds
 * with frac_digits=3 (msec) would yield frac_value = 100.
 */
time_t asn_GT2time_prec(const GeneralizedTime_t *,
	int *frac_value, int frac_digits,
	struct tm *_optional_tm4fill, int as_gmt);

/*
 * Convert a struct tm into GeneralizedTime.
 * If _optional_gt is not given, this function will try to allocate one.
 * If force_gmt is given, the resulting GeneralizedTime will be forced
 * into a GMT time zone (encoding ends with a "Z").
 * On error, this function returns 0 and sets errno.
 */
GeneralizedTime_t *asn_time2GT(GeneralizedTime_t *_optional_gt,
	const struct tm *, int force_gmt);
GeneralizedTime_t *asn_time2GT_frac(GeneralizedTime_t *_optional_gt,
	const struct tm *, int frac_value, int frac_digits, int force_gmt);

#ifdef __cplusplus
}
#endif

#endif	/* _GeneralizedTime_H_ */
