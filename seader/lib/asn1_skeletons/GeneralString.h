/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_GeneralString_H_
#define	_GeneralString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t GeneralString_t;	/* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_GeneralString;
extern asn_TYPE_operation_t asn_OP_GeneralString;

#define GeneralString_free          OCTET_STRING_free
#define GeneralString_print         OCTET_STRING_print
#define GeneralString_compare       OCTET_STRING_compare
#define GeneralString_constraint    asn_generic_unknown_constraint
#define GeneralString_decode_ber    OCTET_STRING_decode_ber
#define GeneralString_encode_der    OCTET_STRING_encode_der
#define GeneralString_decode_xer    OCTET_STRING_decode_xer_hex
#define GeneralString_encode_xer    OCTET_STRING_encode_xer
#define GeneralString_decode_uper   OCTET_STRING_decode_uper
#define GeneralString_encode_uper   OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _GeneralString_H_ */
