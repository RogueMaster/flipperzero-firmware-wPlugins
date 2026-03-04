/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_ENUMERATED_H_
#define	_ENUMERATED_H_

#include <INTEGER.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef INTEGER_t ENUMERATED_t;		/* Implemented via INTEGER */

extern asn_TYPE_descriptor_t asn_DEF_ENUMERATED;
extern asn_TYPE_operation_t asn_OP_ENUMERATED;

oer_type_decoder_f ENUMERATED_decode_oer;
oer_type_encoder_f ENUMERATED_encode_oer;
per_type_decoder_f ENUMERATED_decode_uper;
per_type_encoder_f ENUMERATED_encode_uper;

#define ENUMERATED_free       ASN__PRIMITIVE_TYPE_free
#define ENUMERATED_print      INTEGER_print
#define ENUMERATED_compare    INTEGER_compare
#define ENUMERATED_constraint asn_generic_no_constraint
#define ENUMERATED_decode_ber ber_decode_primitive
#define ENUMERATED_encode_der INTEGER_encode_der
#define ENUMERATED_decode_xer INTEGER_decode_xer
#define ENUMERATED_encode_xer INTEGER_encode_xer
#define ENUMERATED_random_fill INTEGER_random_fill

#ifdef __cplusplus
}
#endif

#endif	/* _ENUMERATED_H_ */
