/**
 * @file inflate.c
 * @brief Streaming raw DEFLATE (RFC 1951) decompressor.
 *
 * Huffman tables are kept in canonical form (counts plus symbols), as in
 * puff.c, and the decoder is a resumable state machine: it may stop in the
 * middle of a block when the output buffer is full and continue later.
 */
#include "inflate.h"

#include <stdlib.h>
#include <string.h>

#define INFLATE_WINDOW_SIZE 32768
#define INFLATE_INPUT_SIZE  512
#define INFLATE_MAX_BITS    15
#define INFLATE_MAX_LITLEN  288
#define INFLATE_MAX_DIST    30
#define INFLATE_MAX_LENGTHS (INFLATE_MAX_LITLEN + INFLATE_MAX_DIST)

/** Canonical Huffman table: number of codes per bit length and sorted symbols. */
typedef struct {
    uint16_t count[INFLATE_MAX_BITS + 1];
    uint16_t symbol[INFLATE_MAX_LITLEN];
} InflateHuffman;

typedef enum {
    InflateModeIdle,
    InflateModeHead,
    InflateModeStored,
    InflateModeCodes,
    InflateModeMatch,
    InflateModeDone,
    InflateModeFailed,
} InflateMode;

struct ReaderInflate {
    ReaderInflatePull pull;
    void* pull_context;

    uint8_t input[INFLATE_INPUT_SIZE];
    size_t input_pos;
    size_t input_size;
    bool input_eof;

    uint32_t bit_buffer;
    unsigned bit_count;

    InflateHuffman litlen;
    InflateHuffman dist;
    InflateHuffman clen;
    uint8_t lengths[INFLATE_MAX_LENGTHS];

    uint8_t window[INFLATE_WINDOW_SIZE];
    uint32_t window_pos;
    uint32_t window_have;

    InflateMode mode;
    bool last_block;
    uint32_t stored_left;
    unsigned match_length;
    unsigned match_distance;
};

static const uint16_t inflate_length_base[29] = {
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23,  27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

static const uint8_t inflate_length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

static const uint16_t inflate_distance_base[30] = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};

static const uint8_t inflate_distance_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

static const uint8_t inflate_code_length_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

static bool inflate_refill(ReaderInflate* inflate) {
    if(inflate->input_pos < inflate->input_size) {
        return true;
    }
    if(inflate->input_eof) {
        return false;
    }

    size_t size = inflate->pull(inflate->pull_context, inflate->input, sizeof(inflate->input));
    if(size == 0 || size > sizeof(inflate->input)) {
        inflate->input_eof = true;
        return false;
    }

    inflate->input_pos = 0;
    inflate->input_size = size;
    return true;
}

static bool inflate_bits(ReaderInflate* inflate, unsigned need, unsigned* value) {
    uint32_t buffer = inflate->bit_buffer;
    unsigned count = inflate->bit_count;

    while(count < need) {
        if(!inflate_refill(inflate)) {
            return false;
        }
        buffer |= (uint32_t)inflate->input[inflate->input_pos++] << count;
        count += 8;
    }

    inflate->bit_buffer = buffer >> need;
    inflate->bit_count = count - need;
    *value = buffer & ((1u << need) - 1u);
    return true;
}

/** Drop the bits of the current byte, as required before a stored block. */
static void inflate_align(ReaderInflate* inflate) {
    unsigned drop = inflate->bit_count % 8;
    inflate->bit_buffer >>= drop;
    inflate->bit_count -= drop;
}

/** Read one byte of a stored block, first draining whole bytes left in the bit buffer. */
static bool inflate_byte(ReaderInflate* inflate, uint8_t* value) {
    if(inflate->bit_count >= 8) {
        *value = (uint8_t)(inflate->bit_buffer & 0xFFu);
        inflate->bit_buffer >>= 8;
        inflate->bit_count -= 8;
        return true;
    }
    if(!inflate_refill(inflate)) {
        return false;
    }
    *value = inflate->input[inflate->input_pos++];
    return true;
}

/** Build a canonical Huffman table from code lengths. @returns 0 if complete, negative if over-subscribed, positive if incomplete. */
static int inflate_construct(InflateHuffman* h, const uint8_t* lengths, size_t count) {
    uint16_t offsets[INFLATE_MAX_BITS + 1];

    for(unsigned len = 0; len <= INFLATE_MAX_BITS; len++) {
        h->count[len] = 0;
    }
    for(size_t symbol = 0; symbol < count; symbol++) {
        h->count[lengths[symbol]]++;
    }
    if(h->count[0] == count) {
        return 0;
    }

    int left = 1;
    for(unsigned len = 1; len <= INFLATE_MAX_BITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if(left < 0) {
            return left;
        }
    }

    offsets[1] = 0;
    for(unsigned len = 1; len < INFLATE_MAX_BITS; len++) {
        offsets[len + 1] = offsets[len] + h->count[len];
    }
    for(size_t symbol = 0; symbol < count; symbol++) {
        if(lengths[symbol] != 0) {
            h->symbol[offsets[lengths[symbol]]++] = (uint16_t)symbol;
        }
    }

    return left;
}

/** Decode one symbol. @returns symbol, negative on truncated input or an invalid code. */
static int inflate_decode(ReaderInflate* inflate, const InflateHuffman* h) {
    int code = 0;
    int first = 0;
    int index = 0;

    for(unsigned len = 1; len <= INFLATE_MAX_BITS; len++) {
        unsigned bit;
        if(!inflate_bits(inflate, 1, &bit)) {
            return -1;
        }
        code |= (int)bit;

        int count = h->count[len];
        if(code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }

    return -1;
}

/** Accept incomplete tables only when they hold a single one-bit code. */
static bool inflate_complete(const InflateHuffman* h, int error, size_t count) {
    if(error < 0) {
        return false;
    }
    if(error > 0 && (size_t)(h->count[0] + h->count[1]) != count) {
        return false;
    }
    return true;
}

static bool inflate_fixed(ReaderInflate* inflate) {
    uint8_t* lengths = inflate->lengths;

    for(unsigned symbol = 0; symbol < 144; symbol++) {
        lengths[symbol] = 8;
    }
    for(unsigned symbol = 144; symbol < 256; symbol++) {
        lengths[symbol] = 9;
    }
    for(unsigned symbol = 256; symbol < 280; symbol++) {
        lengths[symbol] = 7;
    }
    for(unsigned symbol = 280; symbol < INFLATE_MAX_LITLEN; symbol++) {
        lengths[symbol] = 8;
    }
    int error = inflate_construct(&inflate->litlen, lengths, INFLATE_MAX_LITLEN);
    if(error != 0) {
        return false;
    }

    for(unsigned symbol = 0; symbol < INFLATE_MAX_DIST; symbol++) {
        lengths[symbol] = 5;
    }
    /* The fixed distance table is deliberately incomplete: 30 codes of 5 bits. */
    return inflate_construct(&inflate->dist, lengths, INFLATE_MAX_DIST) >= 0;
}

static bool inflate_dynamic(ReaderInflate* inflate) {
    unsigned nlen;
    unsigned ndist;
    unsigned ncode;
    uint8_t* lengths = inflate->lengths;

    if(!inflate_bits(inflate, 5, &nlen) || !inflate_bits(inflate, 5, &ndist) ||
       !inflate_bits(inflate, 4, &ncode)) {
        return false;
    }
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if(nlen > 286 || ndist > INFLATE_MAX_DIST) {
        return false;
    }

    for(unsigned index = 0; index < ncode; index++) {
        unsigned value;
        if(!inflate_bits(inflate, 3, &value)) {
            return false;
        }
        lengths[inflate_code_length_order[index]] = (uint8_t)value;
    }
    for(unsigned index = ncode; index < 19; index++) {
        lengths[inflate_code_length_order[index]] = 0;
    }

    /* The code length code itself must be complete. */
    int error = inflate_construct(&inflate->clen, lengths, 19);
    if(error != 0) {
        return false;
    }

    size_t index = 0;
    size_t total = (size_t)nlen + ndist;
    while(index < total) {
        int symbol = inflate_decode(inflate, &inflate->clen);
        if(symbol < 0) {
            return false;
        }
        if(symbol < 16) {
            lengths[index++] = (uint8_t)symbol;
            continue;
        }

        unsigned repeat;
        uint8_t value = 0;
        if(symbol == 16) {
            if(index == 0) {
                return false;
            }
            unsigned extra;
            value = lengths[index - 1];
            if(!inflate_bits(inflate, 2, &extra)) {
                return false;
            }
            repeat = 3 + extra;
        } else if(symbol == 17) {
            unsigned extra;
            if(!inflate_bits(inflate, 3, &extra)) {
                return false;
            }
            repeat = 3 + extra;
        } else {
            unsigned extra;
            if(!inflate_bits(inflate, 7, &extra)) {
                return false;
            }
            repeat = 11 + extra;
        }

        if(index + repeat > total) {
            return false;
        }
        while(repeat-- > 0) {
            lengths[index++] = value;
        }
    }

    /* A block without an end-of-block code can never terminate. */
    if(lengths[256] == 0) {
        return false;
    }

    error = inflate_construct(&inflate->litlen, lengths, nlen);
    if(!inflate_complete(&inflate->litlen, error, nlen)) {
        return false;
    }
    error = inflate_construct(&inflate->dist, lengths + nlen, ndist);
    return inflate_complete(&inflate->dist, error, ndist);
}

static bool inflate_stored_header(ReaderInflate* inflate) {
    unsigned length;
    unsigned complement;

    inflate_align(inflate);
    if(!inflate_bits(inflate, 16, &length) || !inflate_bits(inflate, 16, &complement)) {
        return false;
    }
    if(length != (complement ^ 0xFFFFu)) {
        return false;
    }

    inflate->stored_left = length;
    return true;
}

/** Decode a length/distance pair for a match. */
static bool inflate_match(ReaderInflate* inflate, unsigned symbol) {
    symbol -= 257;
    if(symbol >= 29) {
        return false;
    }

    unsigned length = inflate_length_base[symbol];
    if(inflate_length_extra[symbol] != 0) {
        unsigned extra;
        if(!inflate_bits(inflate, inflate_length_extra[symbol], &extra)) {
            return false;
        }
        length += extra;
    }

    int distance_symbol = inflate_decode(inflate, &inflate->dist);
    if(distance_symbol < 0 || distance_symbol >= INFLATE_MAX_DIST) {
        return false;
    }

    unsigned distance = inflate_distance_base[distance_symbol];
    if(inflate_distance_extra[distance_symbol] != 0) {
        unsigned extra;
        if(!inflate_bits(inflate, inflate_distance_extra[distance_symbol], &extra)) {
            return false;
        }
        distance += extra;
    }

    if(distance > inflate->window_have) {
        return false;
    }

    inflate->match_length = length;
    inflate->match_distance = distance;
    return true;
}

static void inflate_put(ReaderInflate* inflate, uint8_t value, uint8_t* out, size_t* out_pos) {
    inflate->window[inflate->window_pos++] = value;
    if(inflate->window_pos == INFLATE_WINDOW_SIZE) {
        inflate->window_pos = 0;
    }
    if(inflate->window_have < INFLATE_WINDOW_SIZE) {
        inflate->window_have++;
    }
    out[(*out_pos)++] = value;
}

ReaderInflate* reader_inflate_alloc(void) {
    return calloc(1, sizeof(ReaderInflate));
}

void reader_inflate_free(ReaderInflate* inflate) {
    free(inflate);
}

void reader_inflate_begin(ReaderInflate* inflate, ReaderInflatePull pull, void* context) {
    if(inflate == NULL) {
        return;
    }

    inflate->pull = pull;
    inflate->pull_context = context;
    inflate->input_pos = 0;
    inflate->input_size = 0;
    inflate->input_eof = false;
    inflate->bit_buffer = 0;
    inflate->bit_count = 0;
    inflate->window_pos = 0;
    inflate->window_have = 0;
    inflate->mode = (pull != NULL) ? InflateModeHead : InflateModeIdle;
    inflate->last_block = false;
    inflate->stored_left = 0;
    inflate->match_length = 0;
    inflate->match_distance = 0;
}

size_t reader_inflate_read(ReaderInflate* inflate, uint8_t* out, size_t out_size) {
    if(inflate == NULL || out == NULL || inflate->mode == InflateModeIdle) {
        return 0;
    }

    size_t out_pos = 0;
    while(out_pos < out_size) {
        switch(inflate->mode) {
        case InflateModeHead: {
            unsigned last;
            unsigned type;
            if(!inflate_bits(inflate, 1, &last) || !inflate_bits(inflate, 2, &type)) {
                inflate->mode = InflateModeFailed;
                break;
            }

            inflate->last_block = last != 0;
            if(type == 0) {
                if(!inflate_stored_header(inflate)) {
                    inflate->mode = InflateModeFailed;
                } else {
                    inflate->mode = InflateModeStored;
                }
            } else if(type == 1) {
                if(!inflate_fixed(inflate)) {
                    inflate->mode = InflateModeFailed;
                } else {
                    inflate->mode = InflateModeCodes;
                }
            } else if(type == 2) {
                if(!inflate_dynamic(inflate)) {
                    inflate->mode = InflateModeFailed;
                } else {
                    inflate->mode = InflateModeCodes;
                }
            } else {
                inflate->mode = InflateModeFailed;
            }
            break;
        }
        case InflateModeStored: {
            if(inflate->stored_left == 0) {
                inflate->mode = inflate->last_block ? InflateModeDone : InflateModeHead;
                break;
            }

            uint8_t value;
            if(!inflate_byte(inflate, &value)) {
                inflate->mode = InflateModeFailed;
                break;
            }
            inflate->stored_left--;
            inflate_put(inflate, value, out, &out_pos);
            break;
        }
        case InflateModeCodes: {
            int symbol = inflate_decode(inflate, &inflate->litlen);
            if(symbol < 0) {
                inflate->mode = InflateModeFailed;
            } else if(symbol < 256) {
                inflate_put(inflate, (uint8_t)symbol, out, &out_pos);
            } else if(symbol == 256) {
                /* End of block: the stream ends only after the final block. */
                inflate->mode = inflate->last_block ? InflateModeDone : InflateModeHead;
            } else if(!inflate_match(inflate, (unsigned)symbol)) {
                inflate->mode = InflateModeFailed;
            } else {
                inflate->mode = InflateModeMatch;
            }
            break;
        }
        case InflateModeMatch: {
            while(inflate->match_length > 0 && out_pos < out_size) {
                uint32_t index =
                    inflate->window_pos + INFLATE_WINDOW_SIZE - inflate->match_distance;
                uint8_t value = inflate->window[index & (INFLATE_WINDOW_SIZE - 1)];
                inflate_put(inflate, value, out, &out_pos);
                inflate->match_length--;
            }
            if(inflate->match_length == 0) {
                inflate->mode = InflateModeCodes;
            }
            break;
        }
        case InflateModeDone:
        case InflateModeFailed:
        case InflateModeIdle:
            return out_pos;
        }
    }

    return out_pos;
}

bool reader_inflate_failed(const ReaderInflate* inflate) {
    return inflate != NULL && inflate->mode == InflateModeFailed;
}
