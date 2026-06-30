/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_ObjectDescriptor_H_
#define	_ObjectDescriptor_H_

#include <GraphicString.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef GraphicString_t ObjectDescriptor_t;  /* Implemented via GraphicString */

extern asn_TYPE_descriptor_t asn_DEF_ObjectDescriptor;
extern asn_TYPE_operation_t asn_OP_ObjectDescriptor;

#define ObjectDescriptor_free         OCTET_STRING_free
#define ObjectDescriptor_print        OCTET_STRING_print_utf8
#define ObjectDescriptor_constraint   asn_generic_unknown_constraint
#define ObjectDescriptor_decode_ber   OCTET_STRING_decode_ber
#define ObjectDescriptor_encode_der   OCTET_STRING_encode_der
#define ObjectDescriptor_decode_xer   OCTET_STRING_decode_xer_utf8
#define ObjectDescriptor_encode_xer   OCTET_STRING_encode_xer_utf8
#define ObjectDescriptor_decode_uper  OCTET_STRING_decode_uper
#define ObjectDescriptor_encode_uper  OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _ObjectDescriptor_H_ */
