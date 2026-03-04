/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_IA5String_H_
#define	_IA5String_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t IA5String_t;  /* Implemented via OCTET STRING */

/*
 * IA5String ASN.1 type definition.
 */
extern asn_TYPE_descriptor_t asn_DEF_IA5String;
extern asn_TYPE_operation_t asn_OP_IA5String;

asn_constr_check_f IA5String_constraint;

#define IA5String_free          OCTET_STRING_free
#define IA5String_print         OCTET_STRING_print_utf8
#define IA5String_compare       OCTET_STRING_compare
#define IA5String_decode_ber    OCTET_STRING_decode_ber
#define IA5String_encode_der    OCTET_STRING_encode_der
#define IA5String_decode_xer    OCTET_STRING_decode_xer_utf8
#define IA5String_encode_xer    OCTET_STRING_encode_xer_utf8
#define IA5String_decode_uper   OCTET_STRING_decode_uper
#define IA5String_encode_uper   OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _IA5String_H_ */
