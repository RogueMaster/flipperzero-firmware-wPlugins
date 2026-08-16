#pragma once

#include "tpms_protocol.h"

/** Multi-protocol decoder.
 *
 * Takes the "level + duration" intervals the radio produces and turns
 * them into frames. Every protocol whose modulation matches the one set
 * is looked for at the same time: the intervals are split into chips at
 * each protocol's own chip rate, the chips are sliced into bits, and the
 * bits are searched for sync words.
 */
typedef struct TpmsDecoder TpmsDecoder;

typedef void (*TpmsDecoderCallback)(const TpmsFrame* frame, void* context);

TpmsDecoder* tpms_decoder_alloc(TpmsDecoderCallback callback, void* context);
void tpms_decoder_free(TpmsDecoder* decoder);

/** Drop everything half-received: gaps, a change of frequency, a restart. */
void tpms_decoder_reset(TpmsDecoder* decoder);

/** Only protocols using this modulation are looked for. The radio can be
 * configured for one of them at a time, so this has to match the preset
 * the session loaded. */
void tpms_decoder_set_modulation(TpmsDecoder* decoder, TpmsModulation modulation);

TpmsModulation tpms_decoder_get_modulation(const TpmsDecoder* decoder);

/** Feed one interval from the radio. */
void tpms_decoder_feed(TpmsDecoder* decoder, bool level, uint32_t duration);
