/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_VisibleString_H_
#define	_VisibleString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t VisibleString_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_VisibleString;
extern asn_TYPE_operation_t asn_OP_VisibleString;

asn_constr_check_f VisibleString_constraint;

#define VisibleString_free          OCTET_STRING_free
#define VisibleString_print         OCTET_STRING_print
#define VisibleString_compare       OCTET_STRING_compare
#define VisibleString_constraint    VisibleString_constraint
#define VisibleString_decode_ber    OCTET_STRING_decode_ber
#define VisibleString_encode_der    OCTET_STRING_encode_der
#define VisibleString_decode_xer    OCTET_STRING_decode_xer_hex
#define VisibleString_encode_xer    OCTET_STRING_encode_xer
#define VisibleString_decode_uper   OCTET_STRING_decode_uper
#define VisibleString_encode_uper   OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _VisibleString_H_ */
