/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_VideotexString_H_
#define	_VideotexString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t VideotexString_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_VideotexString;
extern asn_TYPE_operation_t asn_OP_VideotexString;

#define VideotexString_free         OCTET_STRING_free
#define VideotexString_print        OCTET_STRING_print
#define VideotexString_compare      OCTET_STRING_compare
#define VideotexString_constraint   asn_generic_unknown_constraint
#define VideotexString_decode_ber   OCTET_STRING_decode_ber
#define VideotexString_encode_der   OCTET_STRING_encode_der
#define VideotexString_decode_xer   OCTET_STRING_decode_xer_hex
#define VideotexString_encode_xer   OCTET_STRING_encode_xer
#define VideotexString_decode_uper  OCTET_STRING_decode_uper
#define VideotexString_encode_uper  OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _VideotexString_H_ */
