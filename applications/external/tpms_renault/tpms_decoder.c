#include "tpms_decoder.h"
#include "tpms_bits.h"

#include <stdlib.h>
#include <string.h>

/* The radio hands over "level + duration" intervals. Each protocol
 * declares a chip duration, so intervals are split into chips at every
 * distinct rate in use — those are the groups below. Within a group the
 * chips are sliced into bits two ways at once, because rtl_433 has two
 * kinds of TPMS modulation: the PCM slicers pass chips through one to
 * one, while MANCHESTER_ZEROBIT decodes pairs of chips first. A
 * Manchester stream can start on either chip of a pair, so both phases
 * run side by side. Every (group, slicer, phase) combination is a stream.
 *
 * Each stream keeps the bits it has produced in a ring buffer and a shift
 * register of the last few. A protocol matches when the register ends
 * with its sync word; that position is written down, and once enough bits
 * have gone by the payload is read back out of the ring and handed to the
 * protocol's own decoder.
 *
 * Writing positions down rather than filling a buffer per match matters:
 * a sync word turns up inside payloads by chance, several protocols share
 * the same preamble, and no match may cost another one its frame.
 *
 * The demodulator polarity is not known in advance, so a sync word is
 * matched both as written and inverted, and a payload is decoded both
 * ways round. On a real 407003VU0B the sync word arrives in normal
 * polarity while the Manchester pairs come inverted, which is exactly
 * this case.
 */

/** Distinct chip rates in use at once. */
#define TPMS_MAX_GROUPS 8

/** Protocols per group and slicer. The 52 us FSK family is the largest. */
#define TPMS_MAX_GROUP_PROTOCOLS 28

/** Slots within a group: the PCM stream and the two Manchester phases. */
#define TPMS_SLOT_NRZ   0
#define TPMS_SLOT_MC0   1
#define TPMS_SLOT_MC1   2
#define TPMS_SLOT_COUNT 3

#define TPMS_MAX_STREAMS (TPMS_MAX_GROUPS * TPMS_SLOT_COUNT)

/** History each stream keeps, in bits. Has to hold the longest payload
 * (the Kia frame, 276 chips) with room to spare. */
#define TPMS_RING_BITS  384
#define TPMS_RING_BYTES (TPMS_RING_BITS / 8)

/** Sync words waiting for their payload to arrive. */
#define TPMS_MAX_PENDING 64

/** Upper bound on the protocol table, for the bookkeeping that keeps a
 * self-similar sync word from arming on every bit of a run. */
#define TPMS_MAX_PROTOCOLS 48

/** Cheap first pass before comparing sync words: a bitmap of the low bits
 * of every sync word in use, inverted ones included. Noise clears it
 * almost always, and only then are full comparisons made. */
#define TPMS_PREFILTER_BITS  12
#define TPMS_PREFILTER_MASK  ((1UL << TPMS_PREFILTER_BITS) - 1)
#define TPMS_PREFILTER_BYTES (1UL << (TPMS_PREFILTER_BITS - 3))

/** A run longer than this is not part of a frame: the longest run any of
 * these protocols has is eight chips. */
#define TPMS_MAX_RUN_CHIPS 12

/** An interval this long is a gap between transmissions at any chip rate. */
#define TPMS_GAP_US 4000

/** How long an identical frame counts as a repeat of the one before, in
 * intervals. A sync word matching at several offsets or in both
 * polarities would otherwise report the same frame more than once. */
#define TPMS_DUPLICATE_WINDOW 300

/* A last filter, the same for every protocol. Half of these frames are
 * protected by nothing stronger than an eight bit checksum, so noise does
 * get through now and then; a reading no tyre could produce is not a
 * frame. The bounds are deliberately wide — a truck tyre runs at 9 bar,
 * and a sensor left in a freezer or lying on a hot brake disc still has
 * to be believed. */
#define TPMS_MIN_KPA_X100 0
#define TPMS_MAX_KPA_X100 120000
#define TPMS_MIN_TEMP_C   (-60)
#define TPMS_MAX_TEMP_C   130

typedef struct {
    uint16_t chip_us;

    uint64_t nrz_reg;
    uint64_t mc_reg[2];
    uint8_t mc_prev[2];
    bool mc_have[2];

    uint8_t nrz_protocols[TPMS_MAX_GROUP_PROTOCOLS];
    uint8_t nrz_count;
    uint8_t mc_protocols[TPMS_MAX_GROUP_PROTOCOLS];
    uint8_t mc_count;
} TpmsGroup;

typedef struct {
    uint8_t bits[TPMS_RING_BYTES];
    uint32_t count; /**< bits produced since the last reset */
    bool row_start; /**< a burst has just begun on this stream */
} TpmsStream;

typedef struct {
    bool active;
    bool inverted;
    uint8_t stream;
    uint8_t protocol;
    uint32_t start; /**< bit index in that stream where the payload begins */
} TpmsPending;

struct TpmsDecoder {
    TpmsDecoderCallback callback;
    void* context;
    TpmsModulation modulation;

    TpmsGroup group[TPMS_MAX_GROUPS];
    uint8_t group_count;

    TpmsStream stream[TPMS_MAX_STREAMS];
    TpmsPending pending[TPMS_MAX_PENDING];
    uint8_t pending_count;
    /* How many matches each stream is waiting on. Almost always zero, and
     * then the whole list can be skipped — which matters, because this
     * runs for every stream on every chip. */
    uint8_t stream_pending[TPMS_MAX_STREAMS];

    uint8_t prefilter[TPMS_PREFILTER_BYTES];

    /* Where each protocol last matched, per slot within its group. */
    uint32_t last_arm[TPMS_MAX_PROTOCOLS][TPMS_SLOT_COUNT];

    /* Duplicate suppression. */
    uint32_t intervals;
    uint32_t last_interval;
    uint8_t last_protocol;
    uint8_t last_raw[TPMS_RAW_MAX];
    uint8_t last_len;
};

static uint64_t tpms_sync_mask(uint8_t bits) {
    return bits >= 64 ? ~0ULL : ((1ULL << bits) - 1);
}

static void tpms_prefilter_add(TpmsDecoder* decoder, uint64_t value, uint8_t bits) {
    /* A sync word shorter than the prefilter leaves its upper bits free:
     * every combination of them has to be marked. */
    const uint8_t known = bits < TPMS_PREFILTER_BITS ? bits : TPMS_PREFILTER_BITS;
    const uint32_t spare = 1UL << (TPMS_PREFILTER_BITS - known);
    const uint32_t base = (uint32_t)(value & ((1ULL << known) - 1));

    for(uint32_t i = 0; i < spare; i++) {
        const uint32_t index = (base | (i << known)) & TPMS_PREFILTER_MASK;
        decoder->prefilter[index >> 3] |= (uint8_t)(1 << (index & 7));
    }
}

static bool tpms_prefilter_hit(const TpmsDecoder* decoder, uint64_t reg) {
    const uint32_t index = (uint32_t)(reg & TPMS_PREFILTER_MASK);
    return (decoder->prefilter[index >> 3] & (1 << (index & 7))) != 0;
}

/** Build the groups and the prefilter for the current modulation. */
static void tpms_decoder_build(TpmsDecoder* decoder) {
    decoder->group_count = 0;
    memset(decoder->prefilter, 0, sizeof(decoder->prefilter));

    for(uint8_t i = 0; i < tpms_protocol_count; i++) {
        const TpmsProtocol* protocol = &tpms_protocols[i];
        if(protocol->modulation != decoder->modulation) continue;
        if(protocol->capture_bits > TPMS_CAPTURE_MAX_BITS) continue;

        TpmsGroup* group = NULL;
        for(uint8_t g = 0; g < decoder->group_count; g++) {
            if(decoder->group[g].chip_us == protocol->chip_us) {
                group = &decoder->group[g];
                break;
            }
        }

        if(!group) {
            if(decoder->group_count >= TPMS_MAX_GROUPS) continue;
            group = &decoder->group[decoder->group_count++];
            memset(group, 0, sizeof(TpmsGroup));
            group->chip_us = protocol->chip_us;
        }

        /* A protocol with no sync word is found by the start of a burst
         * instead, so it must not colour the prefilter. */
        if(protocol->sync_bits == 0) {
            if(protocol->slicer == TpmsSlicerManchester) {
                if(group->mc_count < TPMS_MAX_GROUP_PROTOCOLS) {
                    group->mc_protocols[group->mc_count++] = i;
                }
            } else if(group->nrz_count < TPMS_MAX_GROUP_PROTOCOLS) {
                group->nrz_protocols[group->nrz_count++] = i;
            }
            continue;
        }

        if(protocol->slicer == TpmsSlicerManchester) {
            if(group->mc_count >= TPMS_MAX_GROUP_PROTOCOLS) continue;
            group->mc_protocols[group->mc_count++] = i;
        } else {
            if(group->nrz_count >= TPMS_MAX_GROUP_PROTOCOLS) continue;
            group->nrz_protocols[group->nrz_count++] = i;
        }

        const uint64_t mask = tpms_sync_mask(protocol->sync_bits);
        tpms_prefilter_add(decoder, protocol->sync, protocol->sync_bits);
        tpms_prefilter_add(decoder, (~protocol->sync) & mask, protocol->sync_bits);
    }
}

static void tpms_pending_release(TpmsDecoder* decoder, TpmsPending* entry) {
    entry->active = false;
    if(decoder->pending_count > 0) decoder->pending_count--;
    if(decoder->stream_pending[entry->stream] > 0) decoder->stream_pending[entry->stream]--;
}

static void tpms_pending_drop_stream(TpmsDecoder* decoder, uint8_t stream) {
    if(decoder->stream_pending[stream] == 0) return;
    for(uint8_t i = 0; i < TPMS_MAX_PENDING; i++) {
        if(decoder->pending[i].active && decoder->pending[i].stream == stream) {
            tpms_pending_release(decoder, &decoder->pending[i]);
        }
    }
}

/** Note a sync word match, to be decoded once its payload has arrived.
 *
 * One match per protocol and stream is kept: a preamble of one repeated
 * pattern — the GM sync word is six zero bytes — matches on every bit of
 * the run, and the first of those is the one aligned with the frame. A
 * later match while that one is still collecting is dropped, and so is
 * any match at all once the list is full, because everything already in
 * it is closer to being a frame.
 */
static void tpms_pending_add(
    TpmsDecoder* decoder,
    uint8_t stream,
    uint8_t protocol,
    bool inverted,
    uint32_t start) {
    /* A preamble of one repeated pattern matches on every bit of the run,
     * and the first of those is the one aligned with the frame. */
    const uint8_t slot_index = (uint8_t)(stream % TPMS_SLOT_COUNT);
    if(protocol < TPMS_MAX_PROTOCOLS) {
        const bool continues = decoder->last_arm[protocol][slot_index] + 1 == start;
        decoder->last_arm[protocol][slot_index] = start;
        if(continues) return;
    }

    TpmsPending* slot = NULL;
    for(uint8_t i = 0; i < TPMS_MAX_PENDING; i++) {
        if(!decoder->pending[i].active) {
            slot = &decoder->pending[i];
            break;
        }
    }
    /* Nothing free: everything already waiting is closer to being a
     * frame than this match is. */
    if(!slot) return;

    slot->active = true;
    slot->stream = stream;
    slot->protocol = protocol;
    slot->inverted = inverted;
    slot->start = start;
    decoder->pending_count++;
    decoder->stream_pending[stream]++;
}

static bool tpms_frame_plausible(const TpmsFrame* frame) {
    if(frame->have & TPMS_HAS_PRESSURE) {
        if(frame->pressure_kpa_x100 < TPMS_MIN_KPA_X100) return false;
        if(frame->pressure_kpa_x100 > TPMS_MAX_KPA_X100) return false;
    }
    if(frame->have & TPMS_HAS_TEMP) {
        if(frame->temperature_c < TPMS_MIN_TEMP_C) return false;
        if(frame->temperature_c > TPMS_MAX_TEMP_C) return false;
    }
    return true;
}

/** A frame is ready: report it unless it repeats the one just reported. */
static void tpms_decoder_emit(TpmsDecoder* decoder, const TpmsFrame* frame) {
    const bool same = decoder->last_len == frame->raw_len &&
                      decoder->last_protocol == frame->protocol &&
                      memcmp(decoder->last_raw, frame->raw, frame->raw_len) == 0;
    const bool recent = (decoder->intervals - decoder->last_interval) < TPMS_DUPLICATE_WINDOW;
    if(same && recent) return;

    decoder->last_protocol = frame->protocol;
    decoder->last_len = frame->raw_len;
    memcpy(decoder->last_raw, frame->raw, frame->raw_len);
    decoder->last_interval = decoder->intervals;

    if(decoder->callback) decoder->callback(frame, decoder->context);
}

/** Read a payload back out of the ring and hand it to the protocol, both
 * ways up. */
static void tpms_pending_decode(TpmsDecoder* decoder, TpmsPending* entry) {
    const TpmsProtocol* protocol = &tpms_protocols[entry->protocol];
    const TpmsStream* stream = &decoder->stream[entry->stream];
    const uint16_t nbits = protocol->capture_bits;
    const uint16_t nbytes = (uint16_t)((nbits + 7) / 8);

    uint8_t payload[TPMS_CAPTURE_MAX_BYTES];
    memset(payload, 0, sizeof(payload));

    uint16_t written = 0;
    for(uint16_t i = 0; i < nbits; i++) {
        const uint32_t position = (entry->start + i) % TPMS_RING_BITS;
        tpms_bit_append(payload, &written, tpms_bit_at(stream->bits, (uint16_t)position));
    }

    if(entry->inverted) tpms_bits_invert(payload, nbytes);

    for(uint8_t attempt = 0; attempt < 2; attempt++) {
        if(attempt == 1) tpms_bits_invert(payload, nbytes);

        TpmsFrame frame;
        memset(&frame, 0, sizeof(frame));
        frame.protocol = entry->protocol;

        if(protocol->decode(payload, nbits, &frame) && tpms_frame_plausible(&frame)) {
            tpms_decoder_emit(decoder, &frame);
            return;
        }
    }
}

/** One bit produced by a stream: store it, hand out finished payloads,
 * then look for sync words. */
static void tpms_stream_bit(
    TpmsDecoder* decoder,
    uint8_t index,
    uint64_t reg,
    uint8_t bit,
    const uint8_t* protocols,
    uint8_t count) {
    TpmsStream* stream = &decoder->stream[index];

    /* Protocols with no sync word of their own start at the first bit of
     * a burst. Only OOK has real bursts — off air means silence — which
     * is where those protocols live. */
    if(stream->row_start) {
        stream->row_start = false;
        for(uint8_t i = 0; i < count; i++) {
            if(tpms_protocols[protocols[i]].sync_bits == 0) {
                tpms_pending_add(decoder, index, protocols[i], false, stream->count);
            }
        }
    }

    const uint32_t position = stream->count % TPMS_RING_BITS;
    if(bit) {
        stream->bits[position >> 3] |= (uint8_t)(0x80 >> (position & 7));
    } else {
        stream->bits[position >> 3] &= (uint8_t) ~(0x80 >> (position & 7));
    }
    stream->count++;

    if(decoder->stream_pending[index] > 0) {
        uint8_t left = decoder->stream_pending[index];
        for(uint8_t i = 0; i < TPMS_MAX_PENDING && left > 0; i++) {
            TpmsPending* entry = &decoder->pending[i];
            if(!entry->active || entry->stream != index) continue;
            left--;
            if(stream->count - entry->start < tpms_protocols[entry->protocol].capture_bits) {
                continue;
            }

            tpms_pending_release(decoder, entry);
            tpms_pending_decode(decoder, entry);
        }
    }

    if(!tpms_prefilter_hit(decoder, reg)) return;

    for(uint8_t i = 0; i < count; i++) {
        const TpmsProtocol* protocol = &tpms_protocols[protocols[i]];
        if(protocol->sync_bits == 0) continue;
        /* The register starts out zeroed, so until it has been filled it
         * would match any sync word made of zeros — the GM preamble is
         * six zero bytes — at the very first bit. */
        if(stream->count < protocol->sync_bits) continue;

        const uint64_t mask = tpms_sync_mask(protocol->sync_bits);
        const uint64_t value = reg & mask;

        if(value == protocol->sync) {
            tpms_pending_add(decoder, index, protocols[i], false, stream->count);
        } else if(value == ((~protocol->sync) & mask)) {
            tpms_pending_add(decoder, index, protocols[i], true, stream->count);
        }
    }
}

static void tpms_group_feed_chip(TpmsDecoder* decoder, uint8_t index, uint8_t chip) {
    TpmsGroup* group = &decoder->group[index];
    const uint8_t base = (uint8_t)(index * TPMS_SLOT_COUNT);

    if(group->nrz_count > 0) {
        group->nrz_reg = (group->nrz_reg << 1) | chip;
        tpms_stream_bit(
            decoder,
            (uint8_t)(base + TPMS_SLOT_NRZ),
            group->nrz_reg,
            chip,
            group->nrz_protocols,
            group->nrz_count);
    }

    if(group->mc_count == 0) return;

    for(uint8_t phase = 0; phase < 2; phase++) {
        if(!group->mc_have[phase]) {
            group->mc_prev[phase] = chip;
            group->mc_have[phase] = true;
            continue;
        }
        group->mc_have[phase] = false;

        if(group->mc_prev[phase] == chip) {
            /* Two equal chips break Manchester coding: whatever this
             * phase was collecting is not a frame. */
            group->mc_reg[phase] = 0;
            tpms_pending_drop_stream(decoder, (uint8_t)(base + TPMS_SLOT_MC0 + phase));
            continue;
        }

        /* rtl_433's Manchester slicer calls a falling data edge a one, so
         * the bit is the first chip of the pair. Which of the two is
         * taken only turns the whole stream over, and both polarities are
         * tried anyway, but keeping the same convention makes the sync
         * words match the ones in rtl_433's decoders. */
        const uint8_t value = group->mc_prev[phase];
        group->mc_reg[phase] = (group->mc_reg[phase] << 1) | value;
        tpms_stream_bit(
            decoder,
            (uint8_t)(base + TPMS_SLOT_MC0 + phase),
            group->mc_reg[phase],
            value,
            group->mc_protocols,
            group->mc_count);
    }
}

static void tpms_group_reset(TpmsDecoder* decoder, uint8_t index) {
    TpmsGroup* group = &decoder->group[index];
    group->nrz_reg = 0;
    group->mc_reg[0] = 0;
    group->mc_reg[1] = 0;
    group->mc_have[0] = false;
    /* The two phases are half a bit apart: one of them starts mid-pair. */
    group->mc_have[1] = true;
    group->mc_prev[0] = 0;
    group->mc_prev[1] = 0;

    for(uint8_t slot = 0; slot < TPMS_SLOT_COUNT; slot++) {
        const uint8_t stream = (uint8_t)(index * TPMS_SLOT_COUNT + slot);
        tpms_pending_drop_stream(decoder, stream);
        decoder->stream[stream].row_start = true;
    }
}

TpmsDecoder* tpms_decoder_alloc(TpmsDecoderCallback callback, void* context) {
    TpmsDecoder* decoder = malloc(sizeof(TpmsDecoder));
    if(!decoder) return NULL;

    memset(decoder, 0, sizeof(TpmsDecoder));
    decoder->callback = callback;
    decoder->context = context;
    decoder->modulation = TpmsModulationFsk;
    tpms_decoder_build(decoder);
    tpms_decoder_reset(decoder);
    return decoder;
}

void tpms_decoder_free(TpmsDecoder* decoder) {
    free(decoder);
}

void tpms_decoder_reset(TpmsDecoder* decoder) {
    for(uint8_t i = 0; i < TPMS_MAX_PENDING; i++)
        decoder->pending[i].active = false;
    decoder->pending_count = 0;
    memset(decoder->stream_pending, 0, sizeof(decoder->stream_pending));
    for(uint8_t i = 0; i < TPMS_MAX_PROTOCOLS; i++) {
        for(uint8_t slot = 0; slot < TPMS_SLOT_COUNT; slot++) {
            /* One short of the largest value, so that "the bit before"
             * can never be the start of a real capture. */
            decoder->last_arm[i][slot] = 0xFFFFFFFEUL;
        }
    }

    for(uint8_t i = 0; i < decoder->group_count; i++)
        tpms_group_reset(decoder, i);
    decoder->last_len = 0;
}

void tpms_decoder_set_modulation(TpmsDecoder* decoder, TpmsModulation modulation) {
    if(decoder->modulation == modulation) return;
    decoder->modulation = modulation;
    tpms_decoder_build(decoder);
    tpms_decoder_reset(decoder);
}

TpmsModulation tpms_decoder_get_modulation(const TpmsDecoder* decoder) {
    return decoder->modulation;
}

void tpms_decoder_feed(TpmsDecoder* decoder, bool level, uint32_t duration) {
    decoder->intervals++;

    if(duration > TPMS_GAP_US) {
        tpms_decoder_reset(decoder);
        return;
    }

    const uint8_t chip = level ? 1 : 0;

    for(uint8_t i = 0; i < decoder->group_count; i++) {
        const uint16_t chip_us = decoder->group[i].chip_us;
        uint32_t chips = (duration + chip_us / 2) / chip_us;
        if(chips == 0) chips = 1;

        if(chips > TPMS_MAX_RUN_CHIPS) {
            /* Too long to be part of a frame at this chip rate — though it
             * may well be an ordinary run at another one. */
            tpms_group_reset(decoder, i);
            continue;
        }

        for(uint32_t c = 0; c < chips; c++)
            tpms_group_feed_chip(decoder, i, chip);
    }
}
