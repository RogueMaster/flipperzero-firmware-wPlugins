/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_PrintableString_H_
#define	_PrintableString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t PrintableString_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_PrintableString;
extern asn_TYPE_operation_t asn_OP_PrintableString;

asn_constr_check_f PrintableString_constraint;

#define PrintableString_free            OCTET_STRING_free
#define PrintableString_print           OCTET_STRING_print_utf8
#define PrintableString_compare         OCTET_STRING_compare
#define PrintableString_decode_ber      OCTET_STRING_decode_ber
#define PrintableString_encode_der      OCTET_STRING_encode_der
#define PrintableString_decode_xer      OCTET_STRING_decode_xer_utf8
#define PrintableString_encode_xer      OCTET_STRING_encode_xer_utf8
#define PrintableString_decode_uper     OCTET_STRING_decode_uper
#define PrintableString_encode_uper     OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _PrintableString_H_ */
