/*
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_OBJECT_IDENTIFIER_H_
#define	_OBJECT_IDENTIFIER_H_

#include <asn_application.h>
#include <asn_codecs_prim.h>
#include <OCTET_STRING.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t asn_oid_arc_t;
#define ASN_OID_ARC_MAX (~((asn_oid_arc_t)0))

typedef ASN__PRIMITIVE_TYPE_t OBJECT_IDENTIFIER_t;

extern asn_TYPE_descriptor_t asn_DEF_OBJECT_IDENTIFIER;
extern asn_TYPE_operation_t asn_OP_OBJECT_IDENTIFIER;

asn_struct_print_f OBJECT_IDENTIFIER_print;
asn_constr_check_f OBJECT_IDENTIFIER_constraint;
der_type_encoder_f OBJECT_IDENTIFIER_encode_der;
xer_type_decoder_f OBJECT_IDENTIFIER_decode_xer;
xer_type_encoder_f OBJECT_IDENTIFIER_encode_xer;
asn_random_fill_f  OBJECT_IDENTIFIER_random_fill;

#define OBJECT_IDENTIFIER_free           ASN__PRIMITIVE_TYPE_free
#define OBJECT_IDENTIFIER_compare        OCTET_STRING_compare
#define OBJECT_IDENTIFIER_decode_ber     ber_decode_primitive
#define OBJECT_IDENTIFIER_encode_der     der_encode_primitive
#define OBJECT_IDENTIFIER_decode_oer     oer_decode_primitive
#define OBJECT_IDENTIFIER_encode_oer     oer_encode_primitive
#define OBJECT_IDENTIFIER_decode_uper    OCTET_STRING_decode_uper
#define OBJECT_IDENTIFIER_encode_uper    OCTET_STRING_encode_uper

/**********************************
 * Some handy conversion routines *
 **********************************/

/*
 * This function fills an (arcs) array with OBJECT IDENTIFIER arcs
 * up to specified (arc_slots) elements.
 *
 * EXAMPLE:
 * 	void print_arcs(OBJECT_IDENTIFIER_t *oid) {
 * 		asn_oid_arc_t fixed_arcs[10];	// Try with fixed space first
 * 		asn_oid_arc_t *arcs = fixed_arcs;
 * 		size_t arc_slots = sizeof(fixed_arcs)/sizeof(fixed_arcs[0]); // 10
 * 		ssize_t count;	// Real number of arcs.
 * 		int i;
 *
 * 		count = OBJECT_IDENTIFIER_get_arcs(oid, arcs, arc_slots);
 * 		// If necessary, reallocate arcs array and try again.
 * 		if(count > arc_slots) {
 * 			arc_slots = count;
 * 			arcs = malloc(sizeof(asn_oid_arc_t) * arc_slots);
 * 			if(!arcs) return;
 * 			count = OBJECT_IDENTIFIER_get_arcs(oid, arcs, arc_slots);
 * 			assert(count == arc_slots);
 * 		}
 *
 * 		// Print the contents of the arcs array.
 * 		for(i = 0; i < count; i++)
 * 			printf("%"PRIu32"\n", arcs[i]);
 *
 * 		// Avoid memory leak.
 * 		if(arcs != fixed_arcs) free(arcs);
 * 	}
 *
 * RETURN VALUES:
 * -1/EINVAL:	Invalid arguments (oid is missing)
 * -1/ERANGE:	One or more arcs have value out of array cell type range.
 * >=0:		Number of arcs contained in the OBJECT IDENTIFIER
 *
 * WARNING: The function always returns the actual number of arcs,
 * even if there is no sufficient (arc_slots) provided.
 */
ssize_t OBJECT_IDENTIFIER_get_arcs(const OBJECT_IDENTIFIER_t *oid,
                                   asn_oid_arc_t *arcs, size_t arc_slots);

/*
 * This functions initializes the OBJECT IDENTIFIER object with
 * the given set of arcs.
 * The minimum of two arcs must be present; some restrictions apply.
 * RETURN VALUES:
 * -1/EINVAL:	Invalid arguments
 * -1/ERANGE:	The first two arcs do not conform to ASN.1 restrictions.
 * -1/ENOMEM:	Memory allocation failed
 * 0:		The object was initialized with new arcs.
 */
int OBJECT_IDENTIFIER_set_arcs(OBJECT_IDENTIFIER_t *oid,
                               const asn_oid_arc_t *arcs, size_t arcs_count);


/*
 * Parse the OBJECT IDENTIFIER textual representation ("1.3.6.1.4.1.9363").
 * No arc can exceed the (0..ASN_OID_ARC_MAX, which is the same as UINT32_MAX).
 * This function is not specific to OBJECT IDENTIFIER, it may be used to parse
 * the RELATIVE-OID data, or any other data consisting of dot-separated
 * series of numeric values.
 *
 * If (oid_txt_length == -1), the strlen() will be invoked to determine the
 * size of the (oid_text) string.
 * 
 * After return, the optional (opt_oid_text_end) is set to the character after
 * the last parsed one. (opt_oid_text_end) is never less than (oid_text).
 * 
 * RETURN VALUES:
 *   -1:	Parse error.
 * >= 0:	Number of arcs contained in the OBJECT IDENTIFIER.
 * 
 * WARNING: The function always returns the real number of arcs,
 * even if there is no sufficient (arc_slots) provided.
 * This is useful for (arc_slots) value estimation.
 */
ssize_t OBJECT_IDENTIFIER_parse_arcs(const char *oid_text,
                                     ssize_t oid_txt_length,
                                     asn_oid_arc_t *arcs, size_t arcs_count,
                                     const char **opt_oid_text_end);

/*
 * Internal functions.
 * Used by RELATIVE-OID implementation in particular.
 */

/*
 * Retrieve a single arc of size from the (arcbuf) buffer.
 * RETURN VALUES:
 *  -1: Failed to retrieve the value from the (arcbuf).
 *  >0: Number of bytes consumed from the (arcbuf), <= (arcbuf_len).
 */
ssize_t OBJECT_IDENTIFIER_get_single_arc(const uint8_t *arcbuf,
                                         size_t arcbuf_len,
                                         asn_oid_arc_t *ret_value);

/*
 * Write the unterminated arc value into the (arcbuf) which has the size at
 * least (arcbuf_len).
 * RETURN VALUES:
 *   -1: (arcbuf_len) size is not sufficient to write the value.
 *  <n>: Number of bytes appended to the arcbuf (<= arcbuf_len).
 */
ssize_t OBJECT_IDENTIFIER_set_single_arc(uint8_t *arcbuf, size_t arcbuf_len,
                                         asn_oid_arc_t arc_value);

#ifdef __cplusplus
}
#endif

#endif	/* _OBJECT_IDENTIFIER_H_ */
