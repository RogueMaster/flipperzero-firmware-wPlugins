/*-
 * Copyright (c) 2004-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * This type differs from the standard INTEGER in that it is modelled using
 * the fixed machine type (long, int, short), so it can hold only values of
 * limited length. There is no type (i.e., NativeInteger_t, any integer type
 * will do).
 * This type may be used when integer range is limited by subtype constraints.
 */
#ifndef	_NativeInteger_H_
#define	_NativeInteger_H_

#include <asn_application.h>
#include <INTEGER.h>

#ifdef __cplusplus
extern "C" {
#endif

extern asn_TYPE_descriptor_t asn_DEF_NativeInteger;
extern asn_TYPE_operation_t asn_OP_NativeInteger;

asn_struct_free_f  NativeInteger_free;
asn_struct_print_f NativeInteger_print;
asn_struct_compare_f NativeInteger_compare;
ber_type_decoder_f NativeInteger_decode_ber;
der_type_encoder_f NativeInteger_encode_der;
#ifndef ASN_DISABLE_XER_SUPPORT
xer_type_decoder_f NativeInteger_decode_xer;
xer_type_encoder_f NativeInteger_encode_xer;
#endif
#ifndef ASN_DISABLE_OER_SUPPORT
oer_type_decoder_f NativeInteger_decode_oer;
oer_type_encoder_f NativeInteger_encode_oer;
#endif
#ifndef ASN_DISABLE_PER_SUPPORT
per_type_decoder_f NativeInteger_decode_uper;
per_type_encoder_f NativeInteger_encode_uper;
#endif
#ifndef ASN_DISABLE_RANDOM_FILL
asn_random_fill_f  NativeInteger_random_fill;
#endif

#define NativeInteger_constraint  asn_generic_no_constraint

#ifdef __cplusplus
}
#endif

#endif	/* _NativeInteger_H_ */
