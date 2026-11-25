#pragma once

#include <lib/subghz/protocols/base.h>

#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "pcsg_generic.h"
#include <lib/subghz/blocks/math.h>

#define SUBGHZ_PROTOCOL_POCSAG_NAME "POCSAG"

extern const SubGhzProtocol subghz_protocol_pocsag;

/** Direct zero-copy access to decoded RIC header and message strings for SD offloading. */
void pocsag_decoder_get_decoded_parts(
    void* context,
    const char** out_ric,
    const char** out_msg);
