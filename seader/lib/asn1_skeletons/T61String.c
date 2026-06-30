/*-
 * Copyright (c) 2003 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <T61String.h>

/*
 * T61String basic type description.
 */
static const ber_tlv_tag_t asn_DEF_T61String_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (20 << 2)),	/* [UNIVERSAL 20] IMPLICIT ...*/
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2))	/* ... OCTET STRING */
};
asn_TYPE_operation_t asn_OP_T61String = {
	OCTET_STRING_free,
	OCTET_STRING_print,         /* non-ascii string */
	OCTET_STRING_compare,
	OCTET_STRING_decode_ber,    /* Implemented in terms of OCTET STRING */
	OCTET_STRING_encode_der,
	OCTET_STRING_decode_xer_hex,
	OCTET_STRING_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_oer,
	OCTET_STRING_encode_oer,
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef	ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	OCTET_STRING_decode_uper,
	OCTET_STRING_encode_uper,
#endif	/* ASN_DISABLE_PER_SUPPORT */
	OCTET_STRING_random_fill,
	0	/* Use generic outmost tag fetcher */
};
asn_TYPE_descriptor_t asn_DEF_T61String = {
	"T61String",
	"T61String",
	&asn_OP_T61String,
	asn_DEF_T61String_tags,
	sizeof(asn_DEF_T61String_tags)
	  / sizeof(asn_DEF_T61String_tags[0]) - 1,
	asn_DEF_T61String_tags,
	sizeof(asn_DEF_T61String_tags)
	  / sizeof(asn_DEF_T61String_tags[0]),
	{ 0, 0, asn_generic_unknown_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

