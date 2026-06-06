#pragma once

#include <furi.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <flipper_format/flipper_format.h>
#include <lib/toolbox/level_duration.h>

#include "../defines.h"

#define LAND_ROVER_PROTOCOL_V0_NAME "Land Rover V0"

extern const SubGhzProtocol land_rover_v0_protocol;

void* subghz_protocol_decoder_land_rover_v0_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_land_rover_v0_free(void* context);
void subghz_protocol_decoder_land_rover_v0_reset(void* context);
void subghz_protocol_decoder_land_rover_v0_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_land_rover_v0_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_land_rover_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
    subghz_protocol_decoder_land_rover_v0_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_land_rover_v0_get_string(void* context, FuriString* output);

void* subghz_protocol_encoder_land_rover_v0_alloc(SubGhzEnvironment* environment);
void subghz_protocol_encoder_land_rover_v0_free(void* context);
SubGhzProtocolStatus
    subghz_protocol_encoder_land_rover_v0_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_encoder_land_rover_v0_stop(void* context);
LevelDuration subghz_protocol_encoder_land_rover_v0_yield(void* context);
