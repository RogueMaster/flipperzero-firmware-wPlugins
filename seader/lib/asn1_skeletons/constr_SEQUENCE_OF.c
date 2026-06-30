/*-
 * Copyright (c) 2003, 2004, 2006 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <constr_SEQUENCE_OF.h>
#include <asn_SEQUENCE_OF.h>

/*
 * The DER encoder of the SEQUENCE OF type.
 */
asn_enc_rval_t
SEQUENCE_OF_encode_der(const asn_TYPE_descriptor_t *td, const void *ptr,
                       int tag_mode, ber_tlv_tag_t tag,
                       asn_app_consume_bytes_f *cb, void *app_key) {
    asn_TYPE_member_t *elm = td->elements;
	const asn_anonymous_sequence_ *list = _A_CSEQUENCE_FROM_VOID(ptr);
	size_t computed_size = 0;
	ssize_t encoding_size = 0;
	asn_enc_rval_t erval;
	int edx;

	ASN_DEBUG("Estimating size of SEQUENCE OF %s", td->name);

	/*
	 * Gather the length of the underlying members sequence.
	 */
	for(edx = 0; edx < list->count; edx++) {
		void *memb_ptr = list->array[edx];
		if(!memb_ptr) continue;
		erval = elm->type->op->der_encoder(elm->type, memb_ptr,
			0, elm->tag,
			0, 0);
		if(erval.encoded == -1)
			return erval;
		computed_size += erval.encoded;
	}

	/*
	 * Encode the TLV for the sequence itself.
	 */
	encoding_size = der_write_tags(td, computed_size, tag_mode, 1, tag,
		cb, app_key);
	if(encoding_size == -1) {
		erval.encoded = -1;
		erval.failed_type = td;
		erval.structure_ptr = ptr;
		return erval;
	}

	computed_size += encoding_size;
	if(!cb) {
		erval.encoded = computed_size;
		ASN__ENCODED_OK(erval);
	}

	ASN_DEBUG("Encoding members of SEQUENCE OF %s", td->name);

	/*
	 * Encode all members.
	 */
	for(edx = 0; edx < list->count; edx++) {
		void *memb_ptr = list->array[edx];
		if(!memb_ptr) continue;
		erval = elm->type->op->der_encoder(elm->type, memb_ptr,
			0, elm->tag,
			cb, app_key);
		if(erval.encoded == -1)
			return erval;
		encoding_size += erval.encoded;
	}

	if(computed_size != (size_t)encoding_size) {
		/*
		 * Encoded size is not equal to the computed size.
		 */
		erval.encoded = -1;
		erval.failed_type = td;
		erval.structure_ptr = ptr;
	} else {
		erval.encoded = computed_size;
		erval.structure_ptr = 0;
		erval.failed_type = 0;
	}

	return erval;
}

asn_enc_rval_t
SEQUENCE_OF_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr,
                       int ilevel, enum xer_encoder_flags_e flags,
                       asn_app_consume_bytes_f *cb, void *app_key) {
    asn_enc_rval_t er;
    const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    const asn_TYPE_member_t *elm = td->elements;
    const asn_anonymous_sequence_ *list = _A_CSEQUENCE_FROM_VOID(sptr);
    const char *mname = specs->as_XMLValueList
                            ? 0
                            : ((*elm->name) ? elm->name : elm->type->xml_tag);
    size_t mlen = mname ? strlen(mname) : 0;
    int xcan = (flags & XER_F_CANONICAL);
    int i;

    if(!sptr) ASN__ENCODE_FAILED;

    er.encoded = 0;

    for(i = 0; i < list->count; i++) {
        asn_enc_rval_t tmper;
        void *memb_ptr = list->array[i];
        if(!memb_ptr) continue;

        if(mname) {
            if(!xcan) ASN__TEXT_INDENT(1, ilevel);
            ASN__CALLBACK3("<", 1, mname, mlen, ">", 1);
        }

        tmper = elm->type->op->xer_encoder(elm->type, memb_ptr, ilevel + 1,
                                           flags, cb, app_key);
        if(tmper.encoded == -1) return tmper;
        er.encoded += tmper.encoded;
        if(tmper.encoded == 0 && specs->as_XMLValueList) {
            const char *name = elm->type->xml_tag;
            size_t len = strlen(name);
            if(!xcan) ASN__TEXT_INDENT(1, ilevel + 1);
            ASN__CALLBACK3("<", 1, name, len, "/>", 2);
        }

        if(mname) {
            ASN__CALLBACK3("</", 2, mname, mlen, ">", 1);
        }
    }

    if(!xcan) ASN__TEXT_INDENT(1, ilevel - 1);

    ASN__ENCODED_OK(er);
cb_failed:
    ASN__ENCODE_FAILED;
}

#ifndef ASN_DISABLE_PER_SUPPORT

asn_enc_rval_t
SEQUENCE_OF_encode_uper(const asn_TYPE_descriptor_t *td,
                        const asn_per_constraints_t *constraints,
                        const void *sptr, asn_per_outp_t *po) {
    const asn_anonymous_sequence_ *list;
	const asn_per_constraint_t *ct;
	asn_enc_rval_t er;
	const asn_TYPE_member_t *elm = td->elements;
	size_t encoded_edx;

	if(!sptr) ASN__ENCODE_FAILED;
    list = _A_CSEQUENCE_FROM_VOID(sptr);

    er.encoded = 0;

	ASN_DEBUG("Encoding %s as SEQUENCE OF (%d)", td->name, list->count);

    if(constraints) ct = &constraints->size;
    else if(td->encoding_constraints.per_constraints)
        ct = &td->encoding_constraints.per_constraints->size;
    else ct = 0;

    /* If extensible constraint, check if size is in root */
    if(ct) {
        int not_in_root =
            (list->count < ct->lower_bound || list->count > ct->upper_bound);
        ASN_DEBUG("lb %ld ub %ld %s", ct->lower_bound, ct->upper_bound,
                  ct->flags & APC_EXTENSIBLE ? "ext" : "fix");
        if(ct->flags & APC_EXTENSIBLE) {
            /* Declare whether size is in extension root */
            if(per_put_few_bits(po, not_in_root, 1)) ASN__ENCODE_FAILED;
            if(not_in_root) ct = 0;
        } else if(not_in_root && ct->effective_bits >= 0) {
            ASN__ENCODE_FAILED;
        }

    }

    if(ct && ct->effective_bits >= 0) {
        /* X.691, #19.5: No length determinant */
        if(per_put_few_bits(po, list->count - ct->lower_bound,
                            ct->effective_bits))
            ASN__ENCODE_FAILED;
    } else if(list->count == 0) {
        /* When the list is empty add only the length determinant
         * X.691, #20.6 and #11.9.4.1
         */
        if (uper_put_length(po, 0, 0)) {
            ASN__ENCODE_FAILED;
        }
        ASN__ENCODED_OK(er);
    }

    for(encoded_edx = 0; (ssize_t)encoded_edx < list->count;) {
        ssize_t may_encode;
        size_t edx;
        int need_eom = 0;

        if(ct && ct->effective_bits >= 0) {
            may_encode = list->count;
        } else {
            may_encode =
                uper_put_length(po, list->count - encoded_edx, &need_eom);
            if(may_encode < 0) ASN__ENCODE_FAILED;
        }

        for(edx = encoded_edx; edx < encoded_edx + may_encode; edx++) {
            void *memb_ptr = list->array[edx];
            if(!memb_ptr) ASN__ENCODE_FAILED;
            er = elm->type->op->uper_encoder(
                elm->type, elm->encoding_constraints.per_constraints, memb_ptr,
                po);
            if(er.encoded == -1) ASN__ENCODE_FAILED;
        }

        if(need_eom && uper_put_length(po, 0, 0))
            ASN__ENCODE_FAILED; /* End of Message length */

        encoded_edx += may_encode;
    }

	ASN__ENCODED_OK(er);
}

#endif  /* ASN_DISABLE_PER_SUPPORT */

int
SEQUENCE_OF_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
               const void *bptr) {
    const asn_anonymous_sequence_ *a = _A_CSEQUENCE_FROM_VOID(aptr);
    const asn_anonymous_sequence_ *b = _A_CSEQUENCE_FROM_VOID(bptr);
    ssize_t idx;

    if(a && b) {
        ssize_t common_length = (a->count < b->count ? a->count : b->count);
        for(idx = 0; idx < common_length; idx++) {
            int ret = td->elements->type->op->compare_struct(
                td->elements->type, a->array[idx], b->array[idx]);
            if(ret) return ret;
        }

        if(idx < b->count) /* more elements in b */
            return -1;    /* a is shorter, so put it first */
        if(idx < a->count) return 1;

    } else if(!a) {
        return -1;
    } else if(!b) {
        return 1;
    }

    return 0;
}


asn_TYPE_operation_t asn_OP_SEQUENCE_OF = {
	SEQUENCE_OF_free,
	SEQUENCE_OF_print,
	SEQUENCE_OF_compare,
	SEQUENCE_OF_decode_ber,
	SEQUENCE_OF_encode_der,
	SEQUENCE_OF_decode_xer,
	SEQUENCE_OF_encode_xer,
#ifdef	ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	SEQUENCE_OF_decode_oer, /* Same as SET OF decoder. */
	SEQUENCE_OF_encode_oer, /* Same as SET OF encoder */
#endif  /* ASN_DISABLE_OER_SUPPORT */
#ifdef ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	SEQUENCE_OF_decode_uper, /* Same as SET OF decoder */
	SEQUENCE_OF_encode_uper,
#endif /* ASN_DISABLE_PER_SUPPORT */
	SEQUENCE_OF_random_fill,
	0	/* Use generic outmost tag fetcher */
};

