/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_BOOLEAN_H_
#define	_BOOLEAN_H_

#include <asn_application.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The underlying integer may contain various values, but everything
 * non-zero is capped to 0xff by the DER encoder. The BER decoder may
 * yield non-zero values different from 1, beware.
 */
typedef int BOOLEAN_t;

extern asn_TYPE_descriptor_t asn_DEF_BOOLEAN;
extern asn_TYPE_operation_t asn_OP_BOOLEAN;

asn_struct_free_f BOOLEAN_free;
asn_struct_print_f BOOLEAN_print;
asn_struct_compare_f BOOLEAN_compare;
ber_type_decoder_f BOOLEAN_decode_ber;
der_type_encoder_f BOOLEAN_encode_der;
#ifndef ASN_DISABLE_OER_SUPPORT
oer_type_decoder_f BOOLEAN_decode_oer;
oer_type_encoder_f BOOLEAN_encode_oer;
#endif
#ifndef ASN_DISABLE_PER_SUPPORT
per_type_decoder_f BOOLEAN_decode_uper;
per_type_encoder_f BOOLEAN_encode_uper;
#endif
#ifndef ASN_DISABLE_XER_SUPPORT
xer_type_decoder_f BOOLEAN_decode_xer;
xer_type_encoder_f BOOLEAN_encode_xer;
#endif
#ifndef ASN_DISABLE_RANDOM_FILL
asn_random_fill_f  BOOLEAN_random_fill;
#endif

#define BOOLEAN_constraint     asn_generic_no_constraint

#ifdef __cplusplus
}
#endif

#endif	/* _BOOLEAN_H_ */
