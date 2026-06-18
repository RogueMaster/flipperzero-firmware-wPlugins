#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SEADER_HF_14A_ATS_T0_TA1 (1U << 4)
#define SEADER_HF_14A_ATS_T0_TB1 (1U << 5)
#define SEADER_HF_14A_ATS_T0_TC1 (1U << 6)

typedef struct {
    uint8_t tl;
    uint8_t t0;
    uint8_t ta_1;
    uint8_t tb_1;
    uint8_t tc_1;
    const uint8_t* t1_tk;
    size_t t1_tk_size;
} SeaderHf14aAtsSource;

bool seader_hf_14a_build_ats(
    const SeaderHf14aAtsSource* source,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);
