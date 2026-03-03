/*-
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_CONSTR_SET_H_
#define	_CONSTR_SET_H_

#include <asn_application.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct asn_SET_specifics_s {
	/*
	 * Target structure description.
	 */
	unsigned struct_size;       /* Size of the target structure. */
	unsigned ctx_offset;        /* Offset of the asn_struct_ctx_t member */
	unsigned pres_offset;       /* Offset of _presence_map member */

	/*
	 * Tags to members mapping table (sorted).
	 * Sometimes suitable for DER encoding (untagged CHOICE is present);
	 * if so, tag2el_count will be greater than td->elements_count.
	 */
	const asn_TYPE_tag2member_t *tag2el;
	unsigned tag2el_count;

	/*
	 * Tags to members mapping table, second edition.
	 * Suitable for CANONICAL-XER encoding.
	 */
	const asn_TYPE_tag2member_t *tag2el_cxer;
	unsigned tag2el_cxer_count;

	/*
	 * Extensions-related stuff.
	 */
	int extensible;                             /* Whether SET is extensible */
	const unsigned int *_mandatory_elements;    /* Bitmask of mandatory ones */
} asn_SET_specifics_t;

/*
 * A set specialized functions dealing with the SET type.
 */
asn_struct_free_f SET_free;
asn_struct_print_f SET_print;
asn_struct_compare_f SET_compare;
asn_constr_check_f SET_constraint;
ber_type_decoder_f SET_decode_ber;
der_type_encoder_f SET_encode_der;
xer_type_decoder_f SET_decode_xer;
xer_type_encoder_f SET_encode_xer;
per_type_decoder_f SET_decode_uper;
per_type_encoder_f SET_encode_uper;
asn_random_fill_f  SET_random_fill;
extern asn_TYPE_operation_t asn_OP_SET;

/***********************
 * Some handy helpers. *
 ***********************/

/*
 * Figure out whether the SET member indicated by PR_x has already been decoded.
 * It is very simple bitfield test, despite its visual complexity.
 */
#define	ASN_SET_ISPRESENT(set_ptr, PR_x)				\
	ASN_SET_ISPRESENT2(&((set_ptr)->_presence_map), PR_x)
#define	ASN_SET_ISPRESENT2(map_ptr, PR_x)				\
	(((unsigned int *)(map_ptr))					\
		[(PR_x)	/ (8 * sizeof(unsigned int))]			\
		& (1u << ((8 * sizeof(unsigned int)) - 1			\
		- ((PR_x) % (8 * sizeof(unsigned int))))))

#define	ASN_SET_MKPRESENT(map_ptr, PR_x)				\
	(((unsigned int *)(map_ptr))					\
		[(PR_x)	/ (8 * sizeof(unsigned int))]			\
		|= (1u << ((8 * sizeof(unsigned int)) - 1		\
		- ((PR_x) % (8 * sizeof(unsigned int))))))

#ifdef __cplusplus
}
#endif

#endif	/* _CONSTR_SET_H_ */
