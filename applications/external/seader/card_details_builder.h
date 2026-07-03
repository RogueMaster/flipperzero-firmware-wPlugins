#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef ASN_EMIT_DEBUG
#define ASN_EMIT_DEBUG 0
#endif

#include <CardDetails.h>

bool seader_card_details_build(
    CardDetails_t* card_details,
    uint8_t sak,
    const uint8_t* uid,
    uint8_t uid_len,
    const uint8_t* ats,
    uint8_t ats_len);

void seader_card_details_reset(CardDetails_t* card_details);
