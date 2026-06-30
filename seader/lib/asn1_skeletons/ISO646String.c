/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <ISO646String.h>

/*
 * ISO646String basic type description.
 */
static const ber_tlv_tag_t asn_DEF_ISO646String_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (26 << 2)),	/* [UNIVERSAL 26] IMPLICIT ...*/
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2))	/* ... OCTET STRING */
};
static asn_per_constraints_t asn_DEF_ISO646String_per_constraints = {
	{ APC_CONSTRAINED, 7, 7, 0x20, 0x7e },	/* Value */
	{ APC_SEMI_CONSTRAINED, -1, -1, 0, 0 },	/* Size */
	0, 0
};
asn_TYPE_operation_t asn_OP_ISO646String = {
	OCTET_STRING_free,
	OCTET_STRING_print_utf8,	/* ASCII subset */
	OCTET_STRING_compare,
	OCTET_STRING_decode_ber,    /* Implemented in terms of OCTET STRING */
	OCTET_STRING_encode_der,
	OCTET_STRING_decode_xer_utf8,
	OCTET_STRING_encode_xer_utf8,
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
asn_TYPE_descriptor_t asn_DEF_ISO646String = {
	"ISO646String",
	"ISO646String",
	&asn_OP_ISO646String,
	asn_DEF_ISO646String_tags,
	sizeof(asn_DEF_ISO646String_tags)
	  / sizeof(asn_DEF_ISO646String_tags[0]) - 1,
	asn_DEF_ISO646String_tags,
	sizeof(asn_DEF_ISO646String_tags)
	  / sizeof(asn_DEF_ISO646String_tags[0]),
	{ 0, &asn_DEF_ISO646String_per_constraints, ISO646String_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};
