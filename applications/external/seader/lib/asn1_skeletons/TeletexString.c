/*-
 * Copyright (c) 2003, 2006 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <TeletexString.h>

/*
 * TeletexString basic type description.
 */
static const ber_tlv_tag_t asn_DEF_TeletexString_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (20 << 2)),	/* [UNIVERSAL 20] IMPLICIT ...*/
	(ASN_TAG_CLASS_UNIVERSAL | (4 << 2)),	/* ... OCTET STRING */
};
asn_TYPE_operation_t asn_OP_TeletexString = {
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
asn_TYPE_descriptor_t asn_DEF_TeletexString = {
	"TeletexString",
	"TeletexString",
	&asn_OP_TeletexString,
	asn_DEF_TeletexString_tags,
	sizeof(asn_DEF_TeletexString_tags)
	  / sizeof(asn_DEF_TeletexString_tags[0]) - 1,
	asn_DEF_TeletexString_tags,
	sizeof(asn_DEF_TeletexString_tags)
	  / sizeof(asn_DEF_TeletexString_tags[0]),
	{ 0, 0, asn_generic_unknown_constraint },
	0, 0,	/* No members */
	0	/* No specifics */
};

/*
 * Here is a formal attempt at creating a mapping from TeletexString
 * (T61String) of the latest ASN.1 standard (X.680:2002) into the Unicode
 * character set. -- Lev Walkin <vlm@lionet.info>
 *
 * The first thing to keep in mind is that TeletexString (T61String)
 * is defined in ASN.1, and is not really a T.61 string.
 * The T.61 standard is withdrawn by ITU-T and is no longer an authoritative
 * reference. See http://www.itu.int/rec/T-REC-T.61
 *
 * The X.680 specifies TeletexString (T61String) as a combination of the
 * character sets specified by the registration numbers listed in
 * ISO International Register of Coded Character Sets to be used with
 * Escape Sequences (ISO-2375):
 * 6, 87, 102, 103, 106, 107, 126, 144, 150, 153, 156, 164, 165, 168,
 * plus SPACE and DELETE characters.
 * In addition to that, the X.680 Table 6 NOTE 2 allows using register entries
 * 6 and 156 instead of 102 and 103.
 *
 * The ISO Register itself is available at http://www.itscj.ipsj.or.jp/ISO-IR/
 *
 * #6 is ASCII. http://www.itscj.ipsj.or.jp/ISO-IR/006.pdf
 * 	Escapes into:
 * 		G0: ESC 2/8 4/2 ("(B")
 * 		G1: ESC 2/9 4/2 (")B")
 * 	The range is [0x21 .. 0x7e]. Conversion into Unicode
 *	is simple, because it has one-to-one correspondence.
 * #87 is a "Japanese Graphic Character Set for Information Interchange".
 * 	Is a multiple-byte set of 6877 characters.
 * 	The character set is JIS X 0208-1983 (originally JIS C 6226-1983).
 * 	Escapes into:
 * 		G0: ESC 2/4 4/2 ("$B")
 * 		G1: ESC 2/4 2/9 4/2 ("$)B")
 * 		G2: ESC 2/4 2/10 4/2 ("$*B")
 * 		G3: ESC 2/4 2/11 4/2 ("$+B")
 * #102 is "Teletex Primary Set of Graphic Characters" and is almost ASCII.
 * 	Escapes into:
 * 		G0: ESC 2/8 7/5 ("(u")
 * 		G1: ESC 2/9 7/5 (")u")
 * 		G2: ESC 2/10 7/5 ("*u")
 * 		G3: ESC 2/11 7/5 ("+u")
 *	It is almost identical to ASCII, except for ASCII position for '$'
 * 	(DOLLAR SIGN) is filled with '¤' (CURRENCY SIGN), which is U+00A4.
 * 	Also, ASCII positions for '`', '\', '^', '{', '}', '~' are marked
 * 	as "should not be used".
 * #103 is a supplementary set of characters used in combination with #102.
 * 	Escapes into:
 * 		G0: ESC 2/8 7/6 ("(v")
 * 		G1: ESC 2/9 7/6 (")v")
 * 		G2: ESC 2/10 7/6 ("*v")
 * 		G3: ESC 2/11 7/6 ("+v")
 * 	Some characters in that character set are combining characters,
 * 	which can only be restrictively used with certain basic Latin letters.
 * 	It can be thought of as a subset of #156 with the exception of 4/12
 * 	which is UNDERLINE in #103 and absent in #156.
 * #106 is a primary set of control functions, used in combination with #107.
 * 	Escapes into:
 * 		C0: ESC 2/1 4/5 ("!E")
 * 	This set is so short I can list it here:
 * 		0x08	BS	BACKSPACE	-- same as Unicode
 * 		0x0a	LF	LINE FEED	-- same as Unicode
 * 		0x0c	FF	FORM FEED	-- same as Unicode
 * 		0x0d	CR	CARRIAGE RETURN	-- same as Unicode
 * 		0x0e	LS1	LOCKING SHIFT ONE
 * 		0x0f	LS0	LOCKING SHIFT ZERO
 * 		0x19	SS2	SINGLE SHIFT TWO
 * 		0x1a	SUB	SUBSTITUTE CHARACTER
 * 		0x1b	ESC	ESCAPE		-- same as Unicode
 * 		0x1d	SS3	SINGLE SHIFT THREE
 *	The LS1 and LS0 are two magical functions which, respectively, invoke
 * 	the currently designated G1 or G0 set into positions 2/1 to 7/14
 * 	The SS2 and SS3, respectively, invoke one character of the
 * 	currently designated set G2 and G3.
 * 	The SUB is wholly equivalent to U+001a (SUBSTITUTE)
 * #107 is a supplementary set of control functions, used with #106.
 * 	Escapes into:
 * 		C1: ESC 2/2 4/8 ('"H')
 * 	This set contains three special control codes:
 * 		0x8b	PLD	PARTIAL LINE DOWN	-- similar to <SUB>
 * 		0x8c	PLU	PARTIAL LINE UP		-- sumilar to <SUP>
 * 		0x9b	CSI	CONTROL SEQUENCE INTRODUCER
 * 	This set is so out of world we can probably safely ignore it.
 * #126 is a "Right-hand Part of the Latin/Greek Alphabet".
 * 	Comprises of 90 characters, including accented letters.
 * 	Escapes into:
 * 		G1: ESC 2/13 4/6 ("-F")
 * 		G2: ESC 2/14 4/6 (".F")
 * 		G3: ESC 2/15 4/6 ("/F")
 * 	Note: This Registration is a subset of ISO-IR 227.
 * #144 is a "Cyrillic part of the Latin/Cyrillic Alphabet".
 * 	Comprises of 95 characters.
 * 	Escapes into:
 * 		G1: ESC 2/13 4/12 ("-L")
 * 		G2: ESC 2/14 4/12 (".L")
 * 		G3: ESC 2/15 4/12 ("/L")
 * #150 is a "Greek Primary Set of Graphic Characters".
 * 	Comprises of 94 characters.
 * 	Escapes into:
 * 		G0: ESC 2/8 2/1 4/0 ("(!@")
 * 		G1: ESC 2/9 2/1 4/0 (")!@")
 * 		G2: ESC 2/10 2/1 4/0 ("*!@")
 * 		G3: ESC 2/11 2/1 4/0 ("+!@")
 * #153 is a "Basic Cyrillic Character Set for 8-bit codes".
 * 	Comprises of 68 characters.
 * 	Escapes into:
 * 		G1: ESC 2/13 4/15 ("-O")
 * 		G2: ESC 2/14 4/15 (".O")
 * 		G3: ESC 2/15 4/15 ("/O")
 * #156 is a "Supplementary Set of ISO/IEC 6937:1992" for use with #6
 * 	Comprises of 87 characters.
 * 	Escapes into:
 * 		G1: ESC 2/13 5/2 ("-R")
 * 		G2: ESC 2/14 5/2 (".R")
 * 		G3: ESC 2/15 5/2 ("/R")
 * #164 is a "Hebrew Supplementary Set of Graphic Characters"
 * 	Comprises of 27 characters.
 * 	Escapes into:
 * 		G1: ESC 2/13 5/3 ("-S")
 * 		G2: ESC 2/14 5/3 (".S")
 * 		G3: ESC 2/15 5/3 ("/S")
 * #165 is a set of "Codes of the Chinese graphic character set"
 * 	Is a multiple-byte set of 8446 characters.
 * 	Escapes into:
 * 		G0: ESC 2/4 2/8 4/5 ("$(E")
 * 		G1: ESC 2/4 2/9 4/5 ("$)E")
 * 		G2: ESC 2/4 2/10 4/5 ("$*E")
 * 		G3: ESC 2/4 2/11 4/5 ("$+E")
 * #168 is a "Japanese Graphic Character Set for Information Interchange"
 * 	A multiple-byte set of 6879 characters updated from #87.
 * 	Escapes into:
 * 		G0: ESC 2/6 4/0 ESC 2/4 4/2 ("&@" "$B")
 * 		G1: ESC 2/6 4/0 ESC 2/4 2/9 4/2 ("&@" "$)B")
 * 		G2: ESC 2/6 4/0 ESC 2/4 2/10 4/2 ("&@" "$*B")
 * 		G3: ESC 2/6 4/0 ESC 2/4 2/11 4/2 ("&@" "$+B")
 */

