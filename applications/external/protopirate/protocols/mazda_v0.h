#pragma once

#include <furi.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <flipper_format/flipper_format.h>

#include "../defines.h"

#define MAZDA_PROTOCOL_NAME "Mazda V0"

typedef struct SubGhzProtocolDecoderMazda SubGhzProtocolDecoderMazda;

extern const SubGhzProtocol mazda_v0_protocol;

void* subghz_protocol_decoder_mazda_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_mazda_free(void* context);
void subghz_protocol_decoder_mazda_reset(void* context);
void subghz_protocol_decoder_mazda_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_mazda_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_mazda_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
    subghz_protocol_decoder_mazda_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_mazda_get_string(void* context, FuriString* output);
