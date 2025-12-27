// Created by FatherDivine (GH)
#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_LACROSSE_TX141BV3_NAME "LC_TX141Bv3"

typedef struct WSProtocolDecoderLaCrosse_TX141BV3 WSProtocolDecoderLaCrosse_TX141BV3;
typedef struct WSProtocolEncoderLaCrosse_TX141BV3 WSProtocolEncoderLaCrosse_TX141BV3;

extern const SubGhzProtocolDecoder ws_protocol_lacrosse_tx141bv3_decoder;
extern const SubGhzProtocolEncoder ws_protocol_lacrosse_tx141bv3_encoder;
extern const SubGhzProtocol ws_protocol_lacrosse_tx141bv3;

/**
 * Allocate WSProtocolDecoderLaCrosse_TX141BV3.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return WSProtocolDecoderLaCrosse_TX141BV3* pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 */
void* ws_protocol_decoder_lacrosse_tx141bv3_alloc(SubGhzEnvironment* environment);

/**
 * Free WSProtocolDecoderLaCrosse_TX141BV3.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 */
void ws_protocol_decoder_lacrosse_tx141bv3_free(void* context);

/**
 * Reset decoder WSProtocolDecoderLaCrosse_TX141BV3.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 */
void ws_protocol_decoder_lacrosse_tx141bv3_reset(void* context);

/**
 * Parse a raw sequence of levels and durations received from the air.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void ws_protocol_decoder_lacrosse_tx141bv3_feed(void* context, bool level, uint32_t duration);

/**
 * Getting the hash sum of the last randomly received parcel.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 * @return hash Hash sum
 */
uint32_t ws_protocol_decoder_lacrosse_tx141bv3_get_hash_data(void* context);

/**
 * Serialize data WSProtocolDecoderLaCrosse_TX141BV3.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus ws_protocol_decoder_lacrosse_tx141bv3_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data WSProtocolDecoderLaCrosse_TX141BV3.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus ws_protocol_decoder_lacrosse_tx141bv3_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a WSProtocolDecoderLaCrosse_TX141BV3 instance
 * @param output Resulting text
 */
void ws_protocol_decoder_lacrosse_tx141bv3_get_string(void* context, FuriString* output);
