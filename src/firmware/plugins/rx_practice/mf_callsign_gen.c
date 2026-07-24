#include "mf_callsign_gen.h"

#include <stddef.h>
#include <string.h>

#include "cw.h"

static const char mf_letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char mf_digits[] = "0123456789";

static char mf_pick(MfRxRng* rng, const char* chars, uint8_t count) {
    return chars[mf_rx_rng_bounded(rng, count)];
}

static void mf_call_clear(MfCallsign* call, MfCallsignEntity entity) {
    *call = (MfCallsign){.entity = (uint8_t)entity};
}

static bool mf_call_finish(MfCallsign* call, uint8_t target_len) {
    if(call->text_len != target_len || call->prefix_len == 0U ||
       call->prefix_len > MF_CALLSIGN_PREFIX_MAX || call->prefix_len >= call->text_len)
        return false;
    call->text[call->text_len] = '\0';
    call->prefix[call->prefix_len] = '\0';
    return true;
}

static void mf_call_prefix(MfCallsign* call, const char* prefix, uint8_t length) {
    memcpy(call->prefix, prefix, length);
    memcpy(call->text, prefix, length);
    call->prefix_len = length;
    call->text_len = length;
}

static void mf_call_digit(MfCallsign* call, char digit) {
    call->text[call->text_len++] = digit;
}

static void mf_call_letters(MfCallsign* call, MfRxRng* rng, uint8_t count) {
    while(count-- != 0U) call->text[call->text_len++] = mf_pick(rng, mf_letters, 26U);
}

static MfCallsignEntity mf_pick_entity(MfRxRng* rng) {
    uint32_t roll = mf_rx_rng_bounded(rng, 100U);
    if(roll < 42U) return MfCallsignEntityUs;
    if(roll < 61U) return MfCallsignEntityGermany;
    if(roll < 76U) return MfCallsignEntityItaly;
    if(roll < 91U) return MfCallsignEntityCanada;
    return MfCallsignEntityRomania;
}

static bool mf_make_us(MfRxRng* rng, uint8_t len, MfCallsign* call) {
    uint32_t variant;
    char prefix[2];
    mf_call_clear(call, MfCallsignEntityUs);
    if(len == 6U) {
        prefix[0] = mf_pick(rng, "KW", 2U);
        prefix[1] = mf_pick(rng, mf_letters, 26U);
        mf_call_prefix(call, prefix, 2U);
        mf_call_digit(call, mf_pick(rng, mf_digits, 10U));
        mf_call_letters(call, rng, 3U);
    } else {
        variant = mf_rx_rng_bounded(rng, len == 4U ? 38U : 40U);
        if((len == 4U && variant < 22U) || (len == 5U && variant < 27U)) {
            prefix[0] = mf_pick(rng, "KNW", 3U);
            mf_call_prefix(call, prefix, 1U);
        } else {
            prefix[0] = mf_rx_rng_bounded(rng, 2U) == 0U ? 'A' : mf_pick(rng, "KNW", 3U);
            prefix[1] = prefix[0] == 'A' ? mf_pick(rng, "ABCDEFGHIJKL", 12U) : mf_pick(rng, mf_letters, 26U);
            mf_call_prefix(call, prefix, 2U);
        }
        mf_call_digit(call, mf_pick(rng, mf_digits, 10U));
        mf_call_letters(call, rng, (uint8_t)(len - call->text_len));
    }
    return mf_call_finish(call, len);
}

static bool mf_make_germany(MfRxRng* rng, uint8_t len, MfCallsign* call) {
    char prefix[2] = {'D', 'L'};
    uint32_t roll;
    mf_call_clear(call, MfCallsignEntityGermany);
    if(len == 4U) {
        prefix[1] = mf_pick(rng, "ABCDEFGHIJKLMNOPQR", 18U);
    } else {
        roll = mf_rx_rng_bounded(rng, 83U);
        if(roll < 35U) prefix[1] = 'L';
        else if(roll < 52U) prefix[1] = mf_pick(rng, "KO", 2U);
        else if(roll < 72U) prefix[1] = mf_pick(rng, "FJMG", 4U);
        else prefix[1] = mf_pick(rng, "HCBDNA", 6U);
    }
    mf_call_prefix(call, prefix, 2U);
    mf_call_digit(call, len == 4U ? mf_pick(rng, mf_digits, 10U) : mf_pick(rng, "123456789", 9U));
    mf_call_letters(call, rng, (uint8_t)(len - call->text_len));
    return mf_call_finish(call, len);
}

static bool mf_make_italy(MfRxRng* rng, uint8_t len, MfCallsign* call) {
    char prefix[2] = {'I', '\0'};
    uint32_t variant;
    mf_call_clear(call, MfCallsignEntityItaly);
    if(len == 6U) {
        prefix[1] = mf_pick(rng, "UKZWVN", 6U);
        mf_call_prefix(call, prefix, 2U);
    } else if(len == 5U && mf_rx_rng_bounded(rng, 11U) < 6U) {
        mf_call_prefix(call, prefix, 1U);
    } else {
        variant = mf_rx_rng_bounded(rng, 2U);
        prefix[1] = len == 4U ? mf_pick(rng, "BIOP", 4U) :
                               (variant == 0U ? mf_pick(rng, mf_letters, 26U) : 'A');
        mf_call_prefix(call, prefix, 2U);
    }
    mf_call_digit(call, mf_pick(rng, mf_digits, 10U));
    mf_call_letters(call, rng, (uint8_t)(len - call->text_len));
    return mf_call_finish(call, len);
}

static bool mf_make_canada(MfRxRng* rng, uint8_t len, MfCallsign* call) {
    char prefix[2] = {'V', 'A'};
    uint32_t roll = mf_rx_rng_bounded(rng, 100U);
    mf_call_clear(call, MfCallsignEntityCanada);
    if(len == 4U) {
        prefix[0] = roll < 75U ? 'V' : 'X';
        prefix[1] = prefix[0] == 'V' ? mf_pick(rng, "BCDGX", 5U) : mf_pick(rng, "LMNO", 4U);
    } else if(roll < (len == 6U ? 51U : 40U)) {
        prefix[1] = mf_pick(rng, "AE", 2U);
    } else {
        prefix[1] = mf_pick(rng, "OY", 2U);
    }
    mf_call_prefix(call, prefix, 2U);
    mf_call_digit(call, mf_pick(rng, mf_digits, 10U));
    mf_call_letters(call, rng, (uint8_t)(len - call->text_len));
    return mf_call_finish(call, len);
}

static bool mf_make_romania(MfRxRng* rng, uint8_t len, MfCallsign* call) {
    char prefix[2] = {'Y', 'O'};
    mf_call_clear(call, MfCallsignEntityRomania);
    if(len == 4U) prefix[1] = mf_pick(rng, "PRQ", 3U);
    mf_call_prefix(call, prefix, 2U);
    mf_call_digit(call, len == 4U ? mf_pick(rng, mf_digits, 10U) : mf_pick(rng, "23456789", 8U));
    mf_call_letters(call, rng, (uint8_t)(len - call->text_len));
    return mf_call_finish(call, len);
}

static bool mf_callsign_construct(MfRxRng* rng, MfCallsignEntity entity, uint8_t len, MfCallsign* call) {
    switch(entity) {
    case MfCallsignEntityUs: return mf_make_us(rng, len, call);
    case MfCallsignEntityGermany: return mf_make_germany(rng, len, call);
    case MfCallsignEntityItaly: return mf_make_italy(rng, len, call);
    case MfCallsignEntityCanada: return mf_make_canada(rng, len, call);
    case MfCallsignEntityRomania: return mf_make_romania(rng, len, call);
    default: return false;
    }
}

static bool mf_prefix_same(const MfCallsignGen* gen, const MfCallsign* call) {
    return gen->last_prefix_len == call->prefix_len && call->prefix_len != 0U &&
           memcmp(gen->last_prefix, call->prefix, call->prefix_len) == 0;
}

static bool mf_entity_valid(const MfCallsign* call) {
    char digit = call->text[call->prefix_len];
    if(call->entity == MfCallsignEntityItaly) {
        if(call->prefix_len == 2U && call->prefix[0] == 'I' && call->prefix[1] == 'T' && digit == '9') return false;
        if(call->prefix_len == 2U && call->prefix[0] == 'I' && call->prefix[1] == 'S' && digit == '0') return false;
        if(call->prefix_len == 2U && (call->prefix[1] == 'M' || call->prefix[1] == 'G' || call->prefix[1] == 'P') && digit == '9') return false;
    }
    return true;
}

void mf_callsign_gen_init(MfCallsignGen* gen) {
    if(gen != NULL) *gen = (MfCallsignGen){0};
}

uint8_t mf_callsign_pick_length(MfRxRng* rng) {
    uint32_t roll = mf_rx_rng_bounded(rng, 4U);
    return roll == 0U ? 4U : roll == 1U ? 6U : 5U;
}

bool mf_callsign_valid(const MfCallsign* call, uint8_t target_len) {
    if(call == NULL || target_len < 4U || target_len > MF_CALLSIGN_MAX_LEN || call->text_len != target_len ||
       call->text[target_len] != '\0' || call->prefix_len == 0U || call->prefix_len > MF_CALLSIGN_PREFIX_MAX ||
       call->prefix_len >= target_len || memcmp(call->text, call->prefix, call->prefix_len) != 0 ||
       call->text[call->prefix_len] < '0' || call->text[call->prefix_len] > '9')
        return false;
    for(uint8_t i = 0U; i < target_len; i++) {
        char ch = call->text[i];
        bool digit = ch >= '0' && ch <= '9';
        bool letter = ch >= 'A' && ch <= 'Z';
        if((!digit && !letter) || cw_symbol_count(cw(ch)) == 0U) return false;
        if(i > call->prefix_len && !letter) return false;
    }
    return mf_entity_valid(call);
}

bool mf_callsign_generate(MfCallsignGen* gen, MfRxRng* rng, uint8_t target_len, MfCallsign* out) {
    static const MfCallsignEntity fallback[] = {
        MfCallsignEntityUs, MfCallsignEntityGermany, MfCallsignEntityItaly,
        MfCallsignEntityCanada, MfCallsignEntityRomania};
    MfCallsign candidate;
    if(gen == NULL || rng == NULL || out == NULL || target_len < 4U || target_len > 6U) return false;
    for(uint8_t i = 0U; i < 16U; i++) {
        if(mf_callsign_construct(rng, mf_pick_entity(rng), target_len, &candidate) &&
           mf_callsign_valid(&candidate, target_len) && !mf_prefix_same(gen, &candidate)) {
            *out = candidate;
            memcpy(gen->last_prefix, candidate.prefix, candidate.prefix_len + 1U);
            gen->last_prefix_len = candidate.prefix_len;
            return true;
        }
    }
    for(uint8_t i = 0U; i < sizeof(fallback) / sizeof(fallback[0]); i++) {
        if(mf_callsign_construct(rng, fallback[i], target_len, &candidate) &&
           mf_callsign_valid(&candidate, target_len) && !mf_prefix_same(gen, &candidate)) {
            *out = candidate;
            memcpy(gen->last_prefix, candidate.prefix, candidate.prefix_len + 1U);
            gen->last_prefix_len = candidate.prefix_len;
            return true;
        }
    }
    return false;
}
