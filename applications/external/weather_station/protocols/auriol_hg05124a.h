#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/math.h>

#include "ws_generic.h"

#define WS_PROTOCOL_AURIOL_HG05124A_NAME "Auriol HG02832"

typedef struct WSProtocolDecoderAuriolHG05124A WSProtocolDecoderAuriolHG05124A;
typedef struct WSProtocolEncoderAuriolHG05124A WSProtocolEncoderAuriolHG05124A;

extern const SubGhzProtocolDecoder ws_protocol_auriol_hg05124a_decoder;
extern const SubGhzProtocolEncoder ws_protocol_auriol_hg05124a_encoder;
extern const SubGhzProtocol ws_protocol_auriol_hg05124a;

void* ws_protocol_decoder_auriol_hg05124a_alloc(SubGhzEnvironment* environment);
void ws_protocol_decoder_auriol_hg05124a_free(void* context);
void ws_protocol_decoder_auriol_hg05124a_reset(void* context);
void ws_protocol_decoder_auriol_hg05124a_feed(void* context, bool level, uint32_t duration);
uint8_t ws_protocol_decoder_auriol_hg05124a_get_hash_data(void* context);

SubGhzProtocolStatus ws_protocol_decoder_auriol_hg05124a_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    ws_protocol_decoder_auriol_hg05124a_deserialize(void* context, FlipperFormat* flipper_format);

void ws_protocol_decoder_auriol_hg05124a_get_string(void* context, FuriString* output);
