/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_TeletexString_H_
#define	_TeletexString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t TeletexString_t;	/* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_TeletexString;
extern asn_TYPE_operation_t asn_OP_TeletexString;

#define TeletexString_free          OCTET_STRING_free
#define TeletexString_print         OCTET_STRING_print
#define TeletexString_compare       OCTET_STRING_compare
#define TeletexString_constraint    asn_generic_unknown_constraint
#define TeletexString_decode_ber    OCTET_STRING_decode_ber
#define TeletexString_encode_der    OCTET_STRING_encode_der
#define TeletexString_decode_xer    OCTET_STRING_decode_xer_hex
#define TeletexString_encode_xer    OCTET_STRING_encode_xer
#define TeletexString_decode_uper   OCTET_STRING_decode_uper
#define TeletexString_encode_uper   OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _TeletexString_H_ */
