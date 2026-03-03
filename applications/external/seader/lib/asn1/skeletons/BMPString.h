/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_BMPString_H_
#define	_BMPString_H_

#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OCTET_STRING_t BMPString_t;  /* Implemented via OCTET STRING */

extern asn_TYPE_descriptor_t asn_DEF_BMPString;
extern asn_TYPE_operation_t asn_OP_BMPString;
extern asn_OCTET_STRING_specifics_t asn_SPC_BMPString_specs;

asn_struct_print_f BMPString_print;	/* Human-readable output */
asn_constr_check_f BMPString_constraint;
xer_type_decoder_f BMPString_decode_xer;
xer_type_encoder_f BMPString_encode_xer;

#define BMPString_free          OCTET_STRING_free
#define BMPString_print         BMPString_print
#define BMPString_compare       OCTET_STRING_compare
#define BMPString_decode_ber    OCTET_STRING_decode_ber
#define BMPString_encode_der    OCTET_STRING_encode_der
#define BMPString_decode_uper   OCTET_STRING_decode_uper
#define BMPString_encode_uper   OCTET_STRING_encode_uper

#ifdef __cplusplus
}
#endif

#endif	/* _BMPString_H_ */
