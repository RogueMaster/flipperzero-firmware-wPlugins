#include "hf_14a_session.h"

#include <string.h>

bool seader_hf_14a_build_ats(
    const SeaderHf14aAtsSource* source,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(out_len) {
        *out_len = 0U;
    }

    if(!source || !out || !out_len) {
        return false;
    }

    if(source->tl <= 1U) {
        return true;
    }

    size_t required = 1U + source->t1_tk_size;
    if(source->t0 & SEADER_HF_14A_ATS_T0_TA1) {
        required++;
    }
    if(source->t0 & SEADER_HF_14A_ATS_T0_TB1) {
        required++;
    }
    if(source->t0 & SEADER_HF_14A_ATS_T0_TC1) {
        required++;
    }

    if(source->t1_tk_size && !source->t1_tk) {
        return false;
    }

    if(required > out_size || required > UINT8_MAX) {
        return false;
    }

    size_t len = 0U;
    out[len++] = source->t0;
    if(source->t0 & SEADER_HF_14A_ATS_T0_TA1) {
        out[len++] = source->ta_1;
    }
    if(source->t0 & SEADER_HF_14A_ATS_T0_TB1) {
        out[len++] = source->tb_1;
    }
    if(source->t0 & SEADER_HF_14A_ATS_T0_TC1) {
        out[len++] = source->tc_1;
    }
    if(source->t1_tk_size) {
        memcpy(out + len, source->t1_tk, source->t1_tk_size);
        len += source->t1_tk_size;
    }

    *out_len = len;
    return true;
}
