/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_UniversalString_H_
#define	_UniversalString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t UniversalString_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_UniversalString;
extern asn_TYPE_operation_t asn_OP_UniversalString;
extern asn_OCTET_STRING_specifics_t asn_SPC_UniversalString_specs;

asn_struct_print_f UniversalString_print;	/* Human-readable output */
asn_constr_check_f UniversalString_constraint;
xer_type_decoder_f UniversalString_decode_xer;
xer_type_encoder_f UniversalString_encode_xer;

#define UniversalString_free            OCTET_STRING_free
#define UniversalString_compare         OCTET_STRING_compare
#define UniversalString_decode_ber      OCTET_STRING_decode_ber
#define UniversalString_encode_der      OCTET_STRING_encode_der
#define UniversalString_decode_uper     OCTET_STRING_decode_uper
#define UniversalString_encode_uper     OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _UniversalString_H_ */
