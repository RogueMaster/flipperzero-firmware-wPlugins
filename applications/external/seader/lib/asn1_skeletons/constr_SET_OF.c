/*
 * Copyright (c) 2003-2017 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_internal.h>
#include <constr_SET_OF.h>
#include <asn_SET_OF.h>

/*
 * Number of bytes left for this structure.
 * (ctx->left) indicates the number of bytes _transferred_ for the structure.
 * (size) contains the number of bytes in the buffer passed.
 */
#define	LEFT	((size<(size_t)ctx->left)?size:(size_t)ctx->left)

/*
 * If the subprocessor function returns with an indication that it wants
 * more data, it may well be a fatal decoding problem, because the
 * size is constrained by the <TLV>'s L, even if the buffer size allows
 * reading more data.
 * For example, consider the buffer containing the following TLVs:
 * <T:5><L:1><V> <T:6>...
 * The TLV length clearly indicates that one byte is expected in V, but
 * if the V processor returns with "want more data" even if the buffer
 * contains way more data than the V processor have seen.
 */
#define	SIZE_VIOLATION	(ctx->left >= 0 && (size_t)ctx->left <= size)

/*
 * This macro "eats" the part of the buffer which is definitely "consumed",
 * i.e. was correctly converted into local representation or rightfully skipped.
 */
#undef	ADVANCE
#define	ADVANCE(num_bytes)	do {		\
		size_t num = num_bytes;		\
		ptr = ((const char *)ptr) + num;\
		size -= num;			\
		if(ctx->left >= 0)		\
			ctx->left -= num;	\
		consumed_myself += num;		\
	} while(0)

/*
 * Switch to the next phase of parsing.
 */
#undef	NEXT_PHASE
#undef	PHASE_OUT
#define	NEXT_PHASE(ctx)	do {			\
		ctx->phase++;			\
		ctx->step = 0;			\
	} while(0)
#define	PHASE_OUT(ctx)	do { ctx->phase = 10; } while(0)

/*
 * Return a standardized complex structure.
 */
#undef	RETURN
#define	RETURN(_code)	do {			\
		rval.code = _code;		\
		rval.consumed = consumed_myself;\
		return rval;			\
	} while(0)

/*
 * The decoder of the SET OF type.
 */
asn_dec_rval_t
SET_OF_decode_ber(const asn_codec_ctx_t *opt_codec_ctx,
                  const asn_TYPE_descriptor_t *td, void **struct_ptr,
                  const void *ptr, size_t size, int tag_mode) {
    /*
	 * Bring closer parts of structure description.
	 */
	const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    const asn_TYPE_member_t *elm = td->elements; /* Single one */

    /*
	 * Parts of the structure being constructed.
	 */
	void *st = *struct_ptr;	/* Target structure. */
	asn_struct_ctx_t *ctx;	/* Decoder context */

	ber_tlv_tag_t tlv_tag;	/* T from TLV */
	asn_dec_rval_t rval;	/* Return code from subparsers */

	ssize_t consumed_myself = 0;	/* Consumed bytes from ptr */

	ASN_DEBUG("Decoding %s as SET OF", td->name);
	
	/*
	 * Create the target structure if it is not present already.
	 */
	if(st == 0) {
		st = *struct_ptr = CALLOC(1, specs->struct_size);
		if(st == 0) {
			RETURN(RC_FAIL);
		}
	}

	/*
	 * Restore parsing context.
	 */
	ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);
	
	/*
	 * Start to parse where left previously
	 */
	switch(ctx->phase) {
	case 0:
		/*
		 * PHASE 0.
		 * Check that the set of tags associated with given structure
		 * perfectly fits our expectations.
		 */

		rval = ber_check_tags(opt_codec_ctx, td, ctx, ptr, size,
			tag_mode, 1, &ctx->left, 0);
		if(rval.code != RC_OK) {
			ASN_DEBUG("%s tagging check failed: %d",
				td->name, rval.code);
			return rval;
		}

		if(ctx->left >= 0)
			ctx->left += rval.consumed; /* ?Substracted below! */
		ADVANCE(rval.consumed);

		ASN_DEBUG("Structure consumes %ld bytes, "
			"buffer %ld", (long)ctx->left, (long)size);

		NEXT_PHASE(ctx);
		/* Fall through */
	case 1:
		/*
		 * PHASE 1.
		 * From the place where we've left it previously,
		 * try to decode the next item.
		 */
	  for(;; ctx->step = 0) {
		ssize_t tag_len;	/* Length of TLV's T */

		if(ctx->step & 1)
			goto microphase2;

		/*
		 * MICROPHASE 1: Synchronize decoding.
		 */

		if(ctx->left == 0) {
			ASN_DEBUG("End of SET OF %s", td->name);
			/*
			 * No more things to decode.
			 * Exit out of here.
			 */
			PHASE_OUT(ctx);
			RETURN(RC_OK);
		}

		/*
		 * Fetch the T from TLV.
		 */
		tag_len = ber_fetch_tag(ptr, LEFT, &tlv_tag);
		switch(tag_len) {
		case 0: if(!SIZE_VIOLATION) RETURN(RC_WMORE);
			/* Fall through */
		case -1: RETURN(RC_FAIL);
		}

		if(ctx->left < 0 && ((const uint8_t *)ptr)[0] == 0) {
			if(LEFT < 2) {
				if(SIZE_VIOLATION)
					RETURN(RC_FAIL);
				else
					RETURN(RC_WMORE);
			} else if(((const uint8_t *)ptr)[1] == 0) {
				/*
				 * Found the terminator of the
				 * indefinite length structure.
				 */
				break;
			}
		}

		/* Outmost tag may be unknown and cannot be fetched/compared */
		if(elm->tag != (ber_tlv_tag_t)-1) {
		    if(BER_TAGS_EQUAL(tlv_tag, elm->tag)) {
			/*
			 * The new list member of expected type has arrived.
			 */
		    } else {
			ASN_DEBUG("Unexpected tag %s fixed SET OF %s",
				ber_tlv_tag_string(tlv_tag), td->name);
			ASN_DEBUG("%s SET OF has tag %s",
				td->name, ber_tlv_tag_string(elm->tag));
			RETURN(RC_FAIL);
		    }
		}

		/*
		 * MICROPHASE 2: Invoke the member-specific decoder.
		 */
		ctx->step |= 1;		/* Confirm entering next microphase */
	microphase2:
		
		/*
		 * Invoke the member fetch routine according to member's type
		 */
		rval = elm->type->op->ber_decoder(opt_codec_ctx,
				elm->type, &ctx->ptr, ptr, LEFT, 0);
		ASN_DEBUG("In %s SET OF %s code %d consumed %d",
			td->name, elm->type->name,
			rval.code, (int)rval.consumed);
		switch(rval.code) {
		case RC_OK:
			{
				asn_anonymous_set_ *list = _A_SET_FROM_VOID(st);
				if(ASN_SET_ADD(list, ctx->ptr) != 0)
					RETURN(RC_FAIL);
				else
					ctx->ptr = 0;
			}
			break;
		case RC_WMORE: /* More data expected */
			if(!SIZE_VIOLATION) {
				ADVANCE(rval.consumed);
				RETURN(RC_WMORE);
			}
			/* Fall through */
		case RC_FAIL: /* Fatal error */
			ASN_STRUCT_FREE(*elm->type, ctx->ptr);
			ctx->ptr = 0;
			RETURN(RC_FAIL);
		} /* switch(rval) */
		
		ADVANCE(rval.consumed);
	  }	/* for(all list members) */

		NEXT_PHASE(ctx);
	case 2:
		/*
		 * Read in all "end of content" TLVs.
		 */
		while(ctx->left < 0) {
			if(LEFT < 2) {
				if(LEFT > 0 && ((const char *)ptr)[0] != 0) {
					/* Unexpected tag */
					RETURN(RC_FAIL);
				} else {
					RETURN(RC_WMORE);
				}
			}
			if(((const char *)ptr)[0] == 0
			&& ((const char *)ptr)[1] == 0) {
				ADVANCE(2);
				ctx->left++;
			} else {
				RETURN(RC_FAIL);
			}
		}

		PHASE_OUT(ctx);
	}
	
	RETURN(RC_OK);
}

/*
 * Internally visible buffer holding a single encoded element.
 */
struct _el_buffer {
	uint8_t *buf;
	size_t length;
	size_t allocated_size;
    unsigned bits_unused;
};
/* Append bytes to the above structure */
static int _el_addbytes(const void *buffer, size_t size, void *el_buf_ptr) {
    struct _el_buffer *el_buf = (struct _el_buffer *)el_buf_ptr;

    if(el_buf->length + size > el_buf->allocated_size) {
        size_t new_size = el_buf->allocated_size ? el_buf->allocated_size : 8;
        void *p;

        do {
            new_size <<= 2;
        } while(el_buf->length + size > new_size);

        p = REALLOC(el_buf->buf, new_size);
        if(p) {
            el_buf->buf = p;
            el_buf->allocated_size = new_size;
        } else {
            return -1;
        }
    }

    memcpy(el_buf->buf + el_buf->length, buffer, size);

    el_buf->length += size;
    return 0;
}

static void assert_unused_bits(const struct _el_buffer* p) {
    if(p->length) {
        assert((p->buf[p->length-1] & ~(0xff << p->bits_unused)) == 0);
    } else {
        assert(p->bits_unused == 0);
    }
}

static int _el_buf_cmp(const void *ap, const void *bp) {
    const struct _el_buffer *a = (const struct _el_buffer *)ap;
    const struct _el_buffer *b = (const struct _el_buffer *)bp;
    size_t common_len;
    int ret = 0;

    if(a->length < b->length)
        common_len = a->length;
    else
        common_len = b->length;

    if (a->buf && b->buf) {
        ret = memcmp(a->buf, b->buf, common_len);
    }
    if(ret == 0) {
        if(a->length < b->length)
            ret = -1;
        else if(a->length > b->length)
            ret = 1;
        /* Ignore unused bits. */
        assert_unused_bits(a);
        assert_unused_bits(b);
    }

    return ret;
}

static void
SET_OF__encode_sorted_free(struct _el_buffer *el_buf, size_t count) {
    size_t i;

    for(i = 0; i < count; i++) {
        FREEMEM(el_buf[i].buf);
    }

    FREEMEM(el_buf);
}

enum SET_OF__encode_method {
    SOES_DER,   /* Distinguished Encoding Rules */
    SOES_CUPER  /* Canonical Unaligned Packed Encoding Rules */
};

static struct _el_buffer *
SET_OF__encode_sorted(const asn_TYPE_member_t *elm,
                      const asn_anonymous_set_ *list,
                      enum SET_OF__encode_method method) {
    struct _el_buffer *encoded_els;
    int edx;

    encoded_els =
        (struct _el_buffer *)CALLOC(list->count, sizeof(encoded_els[0]));
    if(encoded_els == NULL) {
        return NULL;
    }

	/*
	 * Encode all members.
	 */
    for(edx = 0; edx < list->count; edx++) {
        const void *memb_ptr = list->array[edx];
        struct _el_buffer *encoding_el = &encoded_els[edx];
        asn_enc_rval_t erval;

        if(!memb_ptr) break;

        /*
		 * Encode the member into the prepared space.
		 */
        switch(method) {
        case SOES_DER:
            erval = elm->type->op->der_encoder(elm->type, memb_ptr, 0, elm->tag,
                                               _el_addbytes, encoding_el);
            break;
        case SOES_CUPER:
            erval = uper_encode(elm->type,
                                elm->encoding_constraints.per_constraints,
                                memb_ptr, _el_addbytes, encoding_el);
            if(erval.encoded != -1) {
                size_t extra_bits = erval.encoded % 8;
                assert(encoding_el->length == (size_t)(erval.encoded + 7) / 8);
                encoding_el->bits_unused = (8 - extra_bits) & 0x7;
            }
            break;
        default:
            assert(!"Unreachable");
            break;
        }
        if(erval.encoded < 0) break;
	}

    if(edx == list->count) {
        /*
         * Sort the encoded elements according to their encoding.
         */
        qsort(encoded_els, list->count, sizeof(encoded_els[0]), _el_buf_cmp);

        return encoded_els;
    } else {
        SET_OF__encode_sorted_free(encoded_els, edx);
        return NULL;
    }
}


/*
 * The DER encoder of the SET OF type.
 */
asn_enc_rval_t
SET_OF_encode_der(const asn_TYPE_descriptor_t *td, const void *sptr,
                  int tag_mode, ber_tlv_tag_t tag, asn_app_consume_bytes_f *cb,
                  void *app_key) {
    const asn_TYPE_member_t *elm = td->elements;
    const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
    size_t computed_size = 0;
    ssize_t encoding_size = 0;
    struct _el_buffer *encoded_els;
    int edx;

	ASN_DEBUG("Estimating size for SET OF %s", td->name);

    /*
     * Gather the length of the underlying members sequence.
     */
    for(edx = 0; edx < list->count; edx++) {
        void *memb_ptr = list->array[edx];
        asn_enc_rval_t erval;

        if(!memb_ptr) ASN__ENCODE_FAILED;

        erval =
            elm->type->op->der_encoder(elm->type, memb_ptr, 0, elm->tag, 0, 0);
        if(erval.encoded == -1) return erval;
        computed_size += erval.encoded;
	}


    /*
     * Encode the TLV for the sequence itself.
     */
    encoding_size =
        der_write_tags(td, computed_size, tag_mode, 1, tag, cb, app_key);
    if(encoding_size < 0) {
        ASN__ENCODE_FAILED;
    }
    computed_size += encoding_size;

    if(!cb || list->count == 0) {
        asn_enc_rval_t erval;
        erval.encoded = computed_size;
        ASN__ENCODED_OK(erval);
    }

    ASN_DEBUG("Encoding members of %s SET OF", td->name);

    /*
     * DER mandates dynamic sorting of the SET OF elements
     * according to their encodings. Build an array of the
     * encoded elements.
     */
    encoded_els = SET_OF__encode_sorted(elm, list, SOES_DER);

    /*
     * Report encoded elements to the application.
     * Dispose of temporary sorted members table.
     */
    for(edx = 0; edx < list->count; edx++) {
        struct _el_buffer *encoded_el = &encoded_els[edx];
        /* Report encoded chunks to the application */
        if(cb(encoded_el->buf, encoded_el->length, app_key) < 0) {
            break;
        } else {
            encoding_size += encoded_el->length;
        }
    }

    SET_OF__encode_sorted_free(encoded_els, list->count);

    if(edx == list->count) {
        asn_enc_rval_t erval;
        assert(computed_size == (size_t)encoding_size);
        erval.encoded = computed_size;
        ASN__ENCODED_OK(erval);
    } else {
        ASN__ENCODE_FAILED;
    }
}

#undef	XER_ADVANCE
#define	XER_ADVANCE(num_bytes)	do {			\
		size_t num = num_bytes;			\
		buf_ptr = ((const char *)buf_ptr) + num;\
		size -= num;				\
		consumed_myself += num;			\
	} while(0)

/*
 * Decode the XER (XML) data.
 */
asn_dec_rval_t
SET_OF_decode_xer(const asn_codec_ctx_t *opt_codec_ctx,
                  const asn_TYPE_descriptor_t *td, void **struct_ptr,
                  const char *opt_mname, const void *buf_ptr, size_t size) {
    /*
	 * Bring closer parts of structure description.
	 */
	const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
	const asn_TYPE_member_t *element = td->elements;
	const char *elm_tag;
	const char *xml_tag = opt_mname ? opt_mname : td->xml_tag;

	/*
	 * ... and parts of the structure being constructed.
	 */
	void *st = *struct_ptr;	/* Target structure. */
	asn_struct_ctx_t *ctx;	/* Decoder context */

	asn_dec_rval_t rval;		/* Return value from a decoder */
	ssize_t consumed_myself = 0;	/* Consumed bytes from ptr */

	/*
	 * Create the target structure if it is not present already.
	 */
	if(st == 0) {
		st = *struct_ptr = CALLOC(1, specs->struct_size);
		if(st == 0) RETURN(RC_FAIL);
	}

	/* Which tag is expected for the downstream */
	if(specs->as_XMLValueList) {
		elm_tag = (specs->as_XMLValueList == 1) ? 0 : "";
	} else {
		elm_tag = (*element->name)
				? element->name : element->type->xml_tag;
	}

	/*
	 * Restore parsing context.
	 */
	ctx = (asn_struct_ctx_t *)((char *)st + specs->ctx_offset);

	/*
	 * Phases of XER/XML processing:
	 * Phase 0: Check that the opening tag matches our expectations.
	 * Phase 1: Processing body and reacting on closing tag.
	 * Phase 2: Processing inner type.
	 */
	for(; ctx->phase <= 2;) {
		pxer_chunk_type_e ch_type;	/* XER chunk type */
		ssize_t ch_size;		/* Chunk size */
		xer_check_tag_e tcv;		/* Tag check value */

		/*
		 * Go inside the inner member of a set.
		 */
		if(ctx->phase == 2) {
			asn_dec_rval_t tmprval;

			/* Invoke the inner type decoder, m.b. multiple times */
			ASN_DEBUG("XER/SET OF element [%s]", elm_tag);
			tmprval = element->type->op->xer_decoder(opt_codec_ctx,
					element->type, &ctx->ptr, elm_tag,
					buf_ptr, size);
			if(tmprval.code == RC_OK) {
				asn_anonymous_set_ *list = _A_SET_FROM_VOID(st);
				if(ASN_SET_ADD(list, ctx->ptr) != 0)
					RETURN(RC_FAIL);
				ctx->ptr = 0;
				XER_ADVANCE(tmprval.consumed);
			} else {
				XER_ADVANCE(tmprval.consumed);
				RETURN(tmprval.code);
			}
			ctx->phase = 1;	/* Back to body processing */
			ASN_DEBUG("XER/SET OF phase => %d", ctx->phase);
			/* Fall through */
		}

		/*
		 * Get the next part of the XML stream.
		 */
		ch_size = xer_next_token(&ctx->context,
			buf_ptr, size, &ch_type);
		if(ch_size == -1) {
            RETURN(RC_FAIL);
        } else {
			switch(ch_type) {
            case PXER_WMORE:
                RETURN(RC_WMORE);
			case PXER_COMMENT:	/* Got XML comment */
			case PXER_TEXT:		/* Ignore free-standing text */
				XER_ADVANCE(ch_size);	/* Skip silently */
				continue;
			case PXER_TAG:
				break;	/* Check the rest down there */
			}
		}

		tcv = xer_check_tag(buf_ptr, ch_size, xml_tag);
		ASN_DEBUG("XER/SET OF: tcv = %d, ph=%d t=%s",
			tcv, ctx->phase, xml_tag);
		switch(tcv) {
		case XCT_CLOSING:
			if(ctx->phase == 0) break;
			ctx->phase = 0;
			/* Fall through */
		case XCT_BOTH:
			if(ctx->phase == 0) {
				/* No more things to decode */
				XER_ADVANCE(ch_size);
				ctx->phase = 3;	/* Phase out */
				RETURN(RC_OK);
			}
			/* Fall through */
		case XCT_OPENING:
			if(ctx->phase == 0) {
				XER_ADVANCE(ch_size);
				ctx->phase = 1;	/* Processing body phase */
				continue;
			}
			/* Fall through */
		case XCT_UNKNOWN_OP:
		case XCT_UNKNOWN_BO:

			ASN_DEBUG("XER/SET OF: tcv=%d, ph=%d", tcv, ctx->phase);
			if(ctx->phase == 1) {
				/*
				 * Process a single possible member.
				 */
				ctx->phase = 2;
				continue;
			}
			/* Fall through */
		default:
			break;
		}

		ASN_DEBUG("Unexpected XML tag in SET OF");
		break;
	}

	ctx->phase = 3;	/* "Phase out" on hard failure */
	RETURN(RC_FAIL);
}



typedef struct xer_tmp_enc_s {
	void *buffer;
	size_t offset;
	size_t size;
} xer_tmp_enc_t;
static int
SET_OF_encode_xer_callback(const void *buffer, size_t size, void *key) {
	xer_tmp_enc_t *t = (xer_tmp_enc_t *)key;
	if(t->offset + size >= t->size) {
		size_t newsize = (t->size << 2) + size;
		void *p = REALLOC(t->buffer, newsize);
		if(!p) return -1;
		t->buffer = p;
		t->size = newsize;
	}
	memcpy((char *)t->buffer + t->offset, buffer, size);
	t->offset += size;
	return 0;
}
static int
SET_OF_xer_order(const void *aptr, const void *bptr) {
	const xer_tmp_enc_t *a = (const xer_tmp_enc_t *)aptr;
	const xer_tmp_enc_t *b = (const xer_tmp_enc_t *)bptr;
	size_t minlen = a->offset;
	int ret;
	if(b->offset < minlen) minlen = b->offset;
	/* Well-formed UTF-8 has this nice lexicographical property... */
	ret = memcmp(a->buffer, b->buffer, minlen);
	if(ret != 0) return ret;
	if(a->offset == b->offset)
		return 0;
	if(a->offset == minlen)
		return -1;
	return 1;
}


asn_enc_rval_t
SET_OF_encode_xer(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
                  enum xer_encoder_flags_e flags, asn_app_consume_bytes_f *cb,
                  void *app_key) {
    asn_enc_rval_t er;
	const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
	const asn_TYPE_member_t *elm = td->elements;
    const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
    const char *mname = specs->as_XMLValueList
		? 0 : ((*elm->name) ? elm->name : elm->type->xml_tag);
	size_t mlen = mname ? strlen(mname) : 0;
	int xcan = (flags & XER_F_CANONICAL);
	xer_tmp_enc_t *encs = 0;
	size_t encs_count = 0;
	void *original_app_key = app_key;
	asn_app_consume_bytes_f *original_cb = cb;
	int i;

	if(!sptr) ASN__ENCODE_FAILED;

	if(xcan) {
		encs = (xer_tmp_enc_t *)MALLOC(list->count * sizeof(encs[0]));
		if(!encs) ASN__ENCODE_FAILED;
		cb = SET_OF_encode_xer_callback;
	}

	er.encoded = 0;

	for(i = 0; i < list->count; i++) {
		asn_enc_rval_t tmper;

		void *memb_ptr = list->array[i];
		if(!memb_ptr) continue;

		if(encs) {
			memset(&encs[encs_count], 0, sizeof(encs[0]));
			app_key = &encs[encs_count];
			encs_count++;
		}

		if(mname) {
			if(!xcan) ASN__TEXT_INDENT(1, ilevel);
			ASN__CALLBACK3("<", 1, mname, mlen, ">", 1);
		}

		if(!xcan && specs->as_XMLValueList == 1)
			ASN__TEXT_INDENT(1, ilevel + 1);
		tmper = elm->type->op->xer_encoder(elm->type, memb_ptr,
				ilevel + (specs->as_XMLValueList != 2),
				flags, cb, app_key);
		if(tmper.encoded == -1) return tmper;
		er.encoded += tmper.encoded;
		if(tmper.encoded == 0 && specs->as_XMLValueList) {
			const char *name = elm->type->xml_tag;
			size_t len = strlen(name);
			ASN__CALLBACK3("<", 1, name, len, "/>", 2);
		}

		if(mname) {
			ASN__CALLBACK3("</", 2, mname, mlen, ">", 1);
		}

	}

	if(!xcan) ASN__TEXT_INDENT(1, ilevel - 1);

	if(encs) {
		xer_tmp_enc_t *enc = encs;
		xer_tmp_enc_t *end = encs + encs_count;
		ssize_t control_size = 0;

		er.encoded = 0;
		cb = original_cb;
		app_key = original_app_key;
		qsort(encs, encs_count, sizeof(encs[0]), SET_OF_xer_order);

		for(; enc < end; enc++) {
			ASN__CALLBACK(enc->buffer, enc->offset);
			FREEMEM(enc->buffer);
			enc->buffer = 0;
			control_size += enc->offset;
		}
		assert(control_size == er.encoded);
	}

	goto cleanup;
cb_failed:
	ASN__ENCODE_FAILED;
cleanup:
	if(encs) {
		size_t n;
		for(n = 0; n < encs_count; n++) {
			FREEMEM(encs[n].buffer);
		}
		FREEMEM(encs);
	}
	ASN__ENCODED_OK(er);
}

int
SET_OF_print(const asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
             asn_app_consume_bytes_f *cb, void *app_key) {
    asn_TYPE_member_t *elm = td->elements;
	const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
	int ret;
	int i;

	if(!sptr) return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	/* Dump preamble */
	if(cb(td->name, strlen(td->name), app_key) < 0
	|| cb(" ::= {", 6, app_key) < 0)
		return -1;

	for(i = 0; i < list->count; i++) {
		const void *memb_ptr = list->array[i];
		if(!memb_ptr) continue;

		_i_INDENT(1);

		ret = elm->type->op->print_struct(elm->type, memb_ptr,
			ilevel + 1, cb, app_key);
		if(ret) return ret;
	}

	ilevel--;
	_i_INDENT(1);

	return (cb("}", 1, app_key) < 0) ? -1 : 0;
}

void
SET_OF_free(const asn_TYPE_descriptor_t *td, void *ptr,
            enum asn_struct_free_method method) {
    if(td && ptr) {
		const asn_SET_OF_specifics_t *specs;
		asn_TYPE_member_t *elm = td->elements;
		asn_anonymous_set_ *list = _A_SET_FROM_VOID(ptr);
		asn_struct_ctx_t *ctx;	/* Decoder context */
		int i;

		/*
		 * Could not use set_of_empty() because of (*free)
		 * incompatibility.
		 */
		for(i = 0; i < list->count; i++) {
			void *memb_ptr = list->array[i];
			if(memb_ptr)
			ASN_STRUCT_FREE(*elm->type, memb_ptr);
		}
		list->count = 0;	/* No meaningful elements left */

		asn_set_empty(list);	/* Remove (list->array) */

		specs = (const asn_SET_OF_specifics_t *)td->specifics;
		ctx = (asn_struct_ctx_t *)((char *)ptr + specs->ctx_offset);
		if(ctx->ptr) {
			ASN_STRUCT_FREE(*elm->type, ctx->ptr);
			ctx->ptr = 0;
		}

        switch(method) {
        case ASFM_FREE_EVERYTHING:
            FREEMEM(ptr);
            break;
        case ASFM_FREE_UNDERLYING:
            break;
        case ASFM_FREE_UNDERLYING_AND_RESET:
            memset(ptr, 0, specs->struct_size);
            break;
        }
    }
}

int
SET_OF_constraint(const asn_TYPE_descriptor_t *td, const void *sptr,
                  asn_app_constraint_failed_f *ctfailcb, void *app_key) {
    const asn_TYPE_member_t *elm = td->elements;
	asn_constr_check_f *constr;
	const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
	int i;

	if(!sptr) {
		ASN__CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}

	constr = elm->encoding_constraints.general_constraints;
	if(!constr) constr = elm->type->encoding_constraints.general_constraints;

	/*
	 * Iterate over the members of an array.
	 * Validate each in turn, until one fails.
	 */
	for(i = 0; i < list->count; i++) {
		const void *memb_ptr = list->array[i];
		int ret;

		if(!memb_ptr) continue;

		ret = constr(elm->type, memb_ptr, ctfailcb, app_key);
		if(ret) return ret;
	}

	return 0;
}

#ifndef ASN_DISABLE_PER_SUPPORT

asn_dec_rval_t
SET_OF_decode_uper(const asn_codec_ctx_t *opt_codec_ctx,
                   const asn_TYPE_descriptor_t *td,
                   const asn_per_constraints_t *constraints, void **sptr,
                   asn_per_data_t *pd) {
    asn_dec_rval_t rv;
	const asn_SET_OF_specifics_t *specs = (const asn_SET_OF_specifics_t *)td->specifics;
    const asn_TYPE_member_t *elm = td->elements; /* Single one */
    void *st = *sptr;
	asn_anonymous_set_ *list;
	const asn_per_constraint_t *ct;
	int repeat = 0;
	ssize_t nelems;

	if(ASN__STACK_OVERFLOW_CHECK(opt_codec_ctx))
		ASN__DECODE_FAILED;

	/*
	 * Create the target structure if it is not present already.
	 */
	if(!st) {
		st = *sptr = CALLOC(1, specs->struct_size);
		if(!st) ASN__DECODE_FAILED;
	}                                                                       
	list = _A_SET_FROM_VOID(st);

	/* Figure out which constraints to use */
	if(constraints) ct = &constraints->size;
	else if(td->encoding_constraints.per_constraints)
		ct = &td->encoding_constraints.per_constraints->size;
	else ct = 0;

	if(ct && ct->flags & APC_EXTENSIBLE) {
		int value = per_get_few_bits(pd, 1);
		if(value < 0) ASN__DECODE_STARVED;
		if(value) ct = 0;	/* Not restricted! */
	}

	if(ct && ct->effective_bits >= 0) {
		/* X.691, #19.5: No length determinant */
		nelems = per_get_few_bits(pd, ct->effective_bits);
		ASN_DEBUG("Preparing to fetch %ld+%ld elements from %s",
			(long)nelems, ct->lower_bound, td->name);
		if(nelems < 0)  ASN__DECODE_STARVED;
		nelems += ct->lower_bound;
	} else {
		nelems = -1;
	}

	do {
		int i;
		if(nelems < 0) {
			nelems = uper_get_length(pd, -1, 0, &repeat);
            ASN_DEBUG("Got to decode %" ASN_PRI_SSIZE " elements (eff %d)",
                      nelems, (int)(ct ? ct->effective_bits : -1));
            if(nelems < 0) ASN__DECODE_STARVED;
		}

		for(i = 0; i < nelems; i++) {
			void *ptr = 0;
			ASN_DEBUG("SET OF %s decoding", elm->type->name);
			rv = elm->type->op->uper_decoder(opt_codec_ctx, elm->type,
				elm->encoding_constraints.per_constraints, &ptr, pd);
			ASN_DEBUG("%s SET OF %s decoded %d, %p",
				td->name, elm->type->name, rv.code, ptr);
			if(rv.code == RC_OK) {
				if(ASN_SET_ADD(list, ptr) == 0) {
                    if(rv.consumed == 0 && nelems > 200) {
                        /* Protect from SET OF NULL compression bombs. */
                        ASN__DECODE_FAILED;
                    }
					continue;
                }
				ASN_DEBUG("Failed to add element into %s",
					td->name);
				/* Fall through */
				rv.code = RC_FAIL;
			} else {
				ASN_DEBUG("Failed decoding %s of %s (SET OF)",
					elm->type->name, td->name);
			}
			if(ptr) ASN_STRUCT_FREE(*elm->type, ptr);
			return rv;
		}

		nelems = -1;	/* Allow uper_get_length() */
	} while(repeat);

	ASN_DEBUG("Decoded %s as SET OF", td->name);

	rv.code = RC_OK;
	rv.consumed = 0;
	return rv;
}

asn_enc_rval_t
SET_OF_encode_uper(const asn_TYPE_descriptor_t *td,
                   const asn_per_constraints_t *constraints, const void *sptr,
                   asn_per_outp_t *po) {
    const asn_anonymous_set_ *list;
    const asn_per_constraint_t *ct;
    const asn_TYPE_member_t *elm = td->elements;
    struct _el_buffer *encoded_els;
    asn_enc_rval_t er;
    size_t encoded_edx;

    if(!sptr) ASN__ENCODE_FAILED;

    list = _A_CSET_FROM_VOID(sptr);

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


    /*
     * Canonical UPER #22.1 mandates dynamic sorting of the SET OF elements
     * according to their encodings. Build an array of the encoded elements.
     */
    encoded_els = SET_OF__encode_sorted(elm, list, SOES_CUPER);

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
            const struct _el_buffer *el = &encoded_els[edx];
            if(asn_put_many_bits(po, el->buf,
                                 (8 * el->length) - el->bits_unused) < 0) {
                break;
            }
        }

        if(need_eom && uper_put_length(po, 0, 0))
            ASN__ENCODE_FAILED; /* End of Message length */

        encoded_edx += may_encode;
    }

    SET_OF__encode_sorted_free(encoded_els, list->count);

    if((ssize_t)encoded_edx == list->count) {
        ASN__ENCODED_OK(er);
    } else {
        ASN__ENCODE_FAILED;
    }
}


#endif  /* ASN_DISABLE_PER_SUPPORT */

struct comparable_ptr {
    const asn_TYPE_descriptor_t *td;
    const void *sptr;
};

static int
SET_OF__compare_cb(const void *aptr, const void *bptr) {
    const struct comparable_ptr *a = aptr;
    const struct comparable_ptr *b = bptr;
    assert(a->td == b->td);
    return a->td->op->compare_struct(a->td, a->sptr, b->sptr);
}

int
SET_OF_compare(const asn_TYPE_descriptor_t *td, const void *aptr,
               const void *bptr) {
    const asn_anonymous_set_ *a = _A_CSET_FROM_VOID(aptr);
    const asn_anonymous_set_ *b = _A_CSET_FROM_VOID(bptr);

    if(a && b) {
        struct comparable_ptr *asorted;
        struct comparable_ptr *bsorted;
        ssize_t common_length;
        ssize_t idx;

        if(a->count == 0) {
            if(b->count) return -1;
            return 0;
        } else if(b->count == 0) {
            return 1;
        }

        asorted = MALLOC(a->count * sizeof(asorted[0]));
        bsorted = MALLOC(b->count * sizeof(bsorted[0]));
        if(!asorted || !bsorted) {
            FREEMEM(asorted);
            FREEMEM(bsorted);
            return -1;
        }

        for(idx = 0; idx < a->count; idx++) {
            asorted[idx].td = td->elements->type;
            asorted[idx].sptr = a->array[idx];
        }

        for(idx = 0; idx < b->count; idx++) {
            bsorted[idx].td = td->elements->type;
            bsorted[idx].sptr = b->array[idx];
        }

        qsort(asorted, a->count, sizeof(asorted[0]), SET_OF__compare_cb);
        qsort(bsorted, b->count, sizeof(bsorted[0]), SET_OF__compare_cb);

        common_length = (a->count < b->count ? a->count : b->count);
        for(idx = 0; idx < common_length; idx++) {
            int ret = td->elements->type->op->compare_struct(
                td->elements->type, asorted[idx].sptr, bsorted[idx].sptr);
            if(ret) {
                FREEMEM(asorted);
                FREEMEM(bsorted);
                return ret;
            }
        }

        FREEMEM(asorted);
        FREEMEM(bsorted);

        if(idx < b->count) /* more elements in b */
            return -1;     /* a is shorter, so put it first */
        if(idx < a->count) return 1;
    } else if(!a) {
        return -1;
    } else if(!b) {
        return 1;
    }

	return 0;
}


asn_TYPE_operation_t asn_OP_SET_OF = {
	SET_OF_free,
	SET_OF_print,
	SET_OF_compare,
	SET_OF_decode_ber,
	SET_OF_encode_der,
	SET_OF_decode_xer,
	SET_OF_encode_xer,
#ifdef ASN_DISABLE_OER_SUPPORT
	0,
	0,
#else
	SET_OF_decode_oer,
	SET_OF_encode_oer,
#endif
#ifdef ASN_DISABLE_PER_SUPPORT
	0,
	0,
#else
	SET_OF_decode_uper,
	SET_OF_encode_uper,
#endif /* ASN_DISABLE_PER_SUPPORT */
	SET_OF_random_fill,
	0	/* Use generic outmost tag fetcher */
};


asn_random_fill_result_t
SET_OF_random_fill(const asn_TYPE_descriptor_t *td, void **sptr,
                   const asn_encoding_constraints_t *constraints,
                   size_t max_length) {
    const asn_SET_OF_specifics_t *specs =
        (const asn_SET_OF_specifics_t *)td->specifics;
    asn_random_fill_result_t res_ok = {ARFILL_OK, 0};
    asn_random_fill_result_t result_failed = {ARFILL_FAILED, 0};
    asn_random_fill_result_t result_skipped = {ARFILL_SKIPPED, 0};
    const asn_TYPE_member_t *elm = td->elements;
    void *st = *sptr;
    long max_elements = 5;
    long slb = 0;   /* Lower size bound */
    long sub = 0;   /* Upper size bound */
    size_t rnd_len;

    if(max_length == 0) return result_skipped;

    if(st == NULL) {
        st = (*sptr = CALLOC(1, specs->struct_size));
        if(st == NULL) {
            return result_failed;
        }
    }

    switch(asn_random_between(0, 6)) {
    case 0: max_elements = 0; break;
    case 1: max_elements = 1; break;
    case 2: max_elements = 5; break;
    case 3: max_elements = max_length; break;
    case 4: max_elements = max_length / 2; break;
    case 5: max_elements = max_length / 4; break;
    default: break;
    }
    sub = slb + max_elements;

    if(!constraints || !constraints->per_constraints)
        constraints = &td->encoding_constraints;
    if(constraints->per_constraints) {
        const asn_per_constraint_t *pc = &constraints->per_constraints->size;
        if(pc->flags & APC_SEMI_CONSTRAINED) {
            slb = pc->lower_bound;
            sub = pc->lower_bound + max_elements;
        } else if(pc->flags & APC_CONSTRAINED) {
            slb = pc->lower_bound;
            sub = pc->upper_bound;
            if(sub - slb > max_elements) sub = slb + max_elements;
        }
    }

    /* Bias towards edges of allowed space */
    switch(asn_random_between(-1, 4)) {
    default:
    case -1:
        /* Prepare lengths somewhat outside of constrained range. */
        if(constraints->per_constraints
           && (constraints->per_constraints->size.flags & APC_EXTENSIBLE)) {
            switch(asn_random_between(0, 5)) {
            default:
            case 0:
                rnd_len = 0;
                break;
            case 1:
                if(slb > 0) {
                    rnd_len = slb - 1;
                } else {
                    rnd_len = 0;
                }
                break;
            case 2:
                rnd_len = asn_random_between(0, slb);
                break;
            case 3:
                if(sub < (ssize_t)max_length) {
                    rnd_len = sub + 1;
                } else {
                    rnd_len = max_length;
                }
                break;
            case 4:
                if(sub < (ssize_t)max_length) {
                    rnd_len = asn_random_between(sub + 1, max_length);
                } else {
                    rnd_len = max_length;
                }
                break;
            case 5:
                rnd_len = max_length;
                break;
            }
            break;
        }
        /* Fall through */
    case 0:
        rnd_len = asn_random_between(slb, sub);
        break;
    case 1:
        if(slb < sub) {
            rnd_len = asn_random_between(slb + 1, sub);
            break;
        }
        /* Fall through */
    case 2:
        rnd_len = asn_random_between(slb, slb);
        break;
    case 3:
        if(slb < sub) {
            rnd_len = asn_random_between(slb, sub - 1);
            break;
        }
        /* Fall through */
    case 4:
        rnd_len = asn_random_between(sub, sub);
        break;
    }

    for(; rnd_len > 0; rnd_len--) {
        asn_anonymous_set_ *list = _A_SET_FROM_VOID(st);
        void *ptr = 0;
        asn_random_fill_result_t tmpres = elm->type->op->random_fill(
            elm->type, &ptr, &elm->encoding_constraints,
            (max_length > res_ok.length ? max_length - res_ok.length : 0)
                / rnd_len);
        switch(tmpres.code) {
        case ARFILL_OK:
            ASN_SET_ADD(list, ptr);
            res_ok.length += tmpres.length;
            break;
        case ARFILL_SKIPPED:
            break;
        case ARFILL_FAILED:
            assert(ptr == 0);
            return tmpres;
        }
    }

    return res_ok;
}

