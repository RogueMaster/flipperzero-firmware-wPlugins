#include "mf_callsign_gen.h"

#include <stddef.h>
#include <string.h>

#include "../../cw.h"

#define MF_SET(offset) ((uint8_t)(0x80U | (offset)))

typedef struct {
    uint8_t weights[3];
    uint8_t first;
    uint8_t second;
} MfCallRule;

typedef struct {
    uint8_t weight;
    uint8_t lengths_digits;
} MfEntityRule;

enum {
    MfDigitsAll,
    MfDigitsNonzero,
    MfDigitsTwoToNine,
    MfDigitsEuropeanRussia,
    MfDigitsAsiaticRussia,
    MfDigitsMainlandSpain,
    MfDigitsNonzeroAfterFour,
    MfDigitsTwoToNineAfterFour,
};

enum {
    MfSetKnw = 0,
    MfSetKw,
    MfSetAl,
    MfSetAz,
    MfSetGermany,
    MfSetKo,
    MfSetFjmg,
    MfSetHcbdna,
    MfSetBiop,
    MfSetUkzwvn,
    MfSetAe,
    MfSetOy,
    MfSetBcdgx,
    MfSetLmno,
    MfSetPrq,
    MfSetAi,
};

static const uint32_t mf_sets[] = {
    0x00402400UL,
    0x00400400UL,
    0x00000fffUL,
    0x03ffffffUL,
    0x0003ffffUL,
    0x00004400UL,
    0x00001260UL,
    0x0000208fUL,
    0x0000c102UL,
    0x02702400UL,
    0x00000011UL,
    0x01004000UL,
    0x0080004eUL,
    0x00007800UL,
    0x00038000UL,
    0x000001ffUL,
};

static const uint16_t mf_digit_masks[] = {
    0x03ffU,
    0x03feU,
    0x03fcU,
    0x00feU,
    0x0301U,
    0x00beU,
};

#define ENTITY(weight, lengths, digits) {weight, (uint8_t)((lengths) | ((digits) << 3U))}

static const MfEntityRule mf_entities[MfCallsignEntityCount] = {
    ENTITY(40, 7, MfDigitsAll),
    ENTITY(18, 7, MfDigitsNonzeroAfterFour),
    ENTITY(14, 7, MfDigitsAll),
    ENTITY(14, 7, MfDigitsAll),
    ENTITY(8, 7, MfDigitsTwoToNineAfterFour),
    ENTITY(16, 7, MfDigitsEuropeanRussia),
    ENTITY(13, 7, MfDigitsMainlandSpain),
    ENTITY(13, 4, MfDigitsAll),
    ENTITY(12, 7, MfDigitsAll),
    ENTITY(12, 7, MfDigitsAll),
    ENTITY(12, 7, MfDigitsNonzero),
    ENTITY(7, 7, MfDigitsNonzero),
    ENTITY(11, 3, MfDigitsAll),
    ENTITY(11, 7, MfDigitsAll),
    ENTITY(10, 7, MfDigitsAll),
    ENTITY(10, 7, MfDigitsAll),
    ENTITY(10, 7, MfDigitsAsiaticRussia),
    ENTITY(7, 7, MfDigitsNonzero),
    ENTITY(9, 7, MfDigitsAll),
    ENTITY(9, 7, MfDigitsAll),
};

static const uint8_t mf_entity_rule_offsets[] = {
    0U,  4U,  9U,  13U, 17U, 19U, 22U, 31U, 46U, 52U, 55U,
    62U, 64U, 65U, 67U, 74U, 80U, 83U, 88U, 89U, 91U,
};

#define RULE(entity, weight4, weight5, weight6, first, second) \
    {{weight4, weight5, weight6}, first, second}

static const MfCallRule mf_rules[] = {
    RULE(MfCallsignEntityUs, 22, 27, 0, MF_SET(MfSetKnw), 0),
    RULE(MfCallsignEntityUs, 8, 7, 0, 'A', MF_SET(MfSetAl)),
    RULE(MfCallsignEntityUs, 8, 6, 0, MF_SET(MfSetKnw), MF_SET(MfSetAz)),
    RULE(MfCallsignEntityUs, 0, 0, 1, MF_SET(MfSetKw), MF_SET(MfSetAz)),

    RULE(MfCallsignEntityGermany, 1, 0, 0, 'D', MF_SET(MfSetGermany)),
    RULE(MfCallsignEntityGermany, 0, 35, 35, 'D', 'L'),
    RULE(MfCallsignEntityGermany, 0, 17, 17, 'D', MF_SET(MfSetKo)),
    RULE(MfCallsignEntityGermany, 0, 20, 20, 'D', MF_SET(MfSetFjmg)),
    RULE(MfCallsignEntityGermany, 0, 11, 11, 'D', MF_SET(MfSetHcbdna)),

    RULE(MfCallsignEntityItaly, 1, 0, 0, 'I', MF_SET(MfSetBiop)),
    RULE(MfCallsignEntityItaly, 0, 6, 0, 'I', 0),
    RULE(MfCallsignEntityItaly, 0, 5, 0, 'I', MF_SET(MfSetAz)),
    RULE(MfCallsignEntityItaly, 0, 0, 1, 'I', MF_SET(MfSetUkzwvn)),

    RULE(MfCallsignEntityCanada, 3, 0, 0, 'V', MF_SET(MfSetBcdgx)),
    RULE(MfCallsignEntityCanada, 1, 0, 0, 'X', MF_SET(MfSetLmno)),
    RULE(MfCallsignEntityCanada, 0, 40, 51, 'V', MF_SET(MfSetAe)),
    RULE(MfCallsignEntityCanada, 0, 5, 5, 'V', MF_SET(MfSetOy)),

    RULE(MfCallsignEntityRomania, 1, 0, 0, 'Y', MF_SET(MfSetPrq)),
    RULE(MfCallsignEntityRomania, 0, 1, 1, 'Y', 'O'),

    RULE(MfCallsignEntityEuropeanRussia, 44, 26, 0, 'R', 0),
    RULE(MfCallsignEntityEuropeanRussia, 48, 56, 41, 'R', MF_SET(MfSetAz)),
    RULE(MfCallsignEntityEuropeanRussia, 8, 18, 59, 'U', MF_SET(MfSetAi)),

    RULE(MfCallsignEntitySpain, 29, 1, 0, 'E', 'D'),
    RULE(MfCallsignEntitySpain, 22, 85, 90, 'E', 'A'),
    RULE(MfCallsignEntitySpain, 20, 1, 0, 'E', 'E'),
    RULE(MfCallsignEntitySpain, 18, 0, 0, 'E', 'F'),
    RULE(MfCallsignEntitySpain, 6, 6, 3, 'E', 'C'),
    RULE(MfCallsignEntitySpain, 3, 6, 4, 'E', 'B'),
    RULE(MfCallsignEntitySpain, 2, 0, 0, 'A', 'M'),
    RULE(MfCallsignEntitySpain, 0, 1, 2, 'E', 'G'),
    RULE(MfCallsignEntitySpain, 0, 0, 1, 'E', 'H'),

    RULE(MfCallsignEntityJapan, 0, 0, 26, 'J', 'A'),
    RULE(MfCallsignEntityJapan, 0, 0, 14, 'J', 'H'),
    RULE(MfCallsignEntityJapan, 0, 0, 8, 'J', 'R'),
    RULE(MfCallsignEntityJapan, 0, 0, 6, 'J', 'E'),
    RULE(MfCallsignEntityJapan, 0, 0, 6, 'J', 'J'),
    RULE(MfCallsignEntityJapan, 0, 0, 6, 'J', 'K'),
    RULE(MfCallsignEntityJapan, 0, 0, 5, 'J', 'F'),
    RULE(MfCallsignEntityJapan, 0, 0, 5, 'J', 'G'),
    RULE(MfCallsignEntityJapan, 0, 0, 4, 'J', 'I'),
    RULE(MfCallsignEntityJapan, 0, 0, 3, 'J', 'L'),
    RULE(MfCallsignEntityJapan, 0, 0, 3, 'J', 'M'),
    RULE(MfCallsignEntityJapan, 0, 0, 2, 'J', 'S'),
    RULE(MfCallsignEntityJapan, 0, 0, 2, 'J', 'Q'),
    RULE(MfCallsignEntityJapan, 0, 0, 2, 'J', 'N'),
    RULE(MfCallsignEntityJapan, 0, 0, 2, 'J', 'O'),

    RULE(MfCallsignEntityPoland, 39, 4, 2, 'S', 'N'),
    RULE(MfCallsignEntityPoland, 19, 5, 0, 'S', 'O'),
    RULE(MfCallsignEntityPoland, 18, 63, 73, 'S', 'P'),
    RULE(MfCallsignEntityPoland, 14, 23, 25, 'S', 'Q'),
    RULE(MfCallsignEntityPoland, 5, 2, 0, 'H', 'F'),
    RULE(MfCallsignEntityPoland, 4, 2, 0, '3', 'Z'),

    RULE(MfCallsignEntityEngland, 6, 56, 0, 'G', 0),
    RULE(MfCallsignEntityEngland, 1, 44, 0, 'M', 0),
    RULE(MfCallsignEntityEngland, 0, 0, 1, '2', 'E'),

    RULE(MfCallsignEntityBrazil, 4, 1, 0, 'P', 'R'),
    RULE(MfCallsignEntityBrazil, 3, 0, 0, 'P', 'X'),
    RULE(MfCallsignEntityBrazil, 3, 2, 1, 'P', 'T'),
    RULE(MfCallsignEntityBrazil, 2, 0, 0, 'P', 'W'),
    RULE(MfCallsignEntityBrazil, 0, 12, 8, 'P', 'Y'),
    RULE(MfCallsignEntityBrazil, 0, 3, 1, 'P', 'P'),
    RULE(MfCallsignEntityBrazil, 0, 0, 10, 'P', 'U'),

    RULE(MfCallsignEntityFinland, 1, 0, 0, 'O', 'G'),
    RULE(MfCallsignEntityFinland, 1, 1, 1, 'O', 'H'),

    RULE(MfCallsignEntityFrance, 1, 1, 0, 'F', 0),

    RULE(MfCallsignEntityCzechRepublic, 1, 1, 1, 'O', 'K'),
    RULE(MfCallsignEntityCzechRepublic, 1, 0, 0, 'O', 'L'),

    RULE(MfCallsignEntityUkraine, 5, 8, 4, 'U', 'T'),
    RULE(MfCallsignEntityUkraine, 5, 0, 1, 'U', 'W'),
    RULE(MfCallsignEntityUkraine, 3, 0, 0, 'U', 'Z'),
    RULE(MfCallsignEntityUkraine, 2, 4, 11, 'U', 'R'),
    RULE(MfCallsignEntityUkraine, 0, 3, 0, 'U', 'X'),
    RULE(MfCallsignEntityUkraine, 0, 2, 4, 'U', 'S'),
    RULE(MfCallsignEntityUkraine, 0, 1, 0, 'U', 'Y'),

    RULE(MfCallsignEntityNetherlands, 6, 9, 11, 'P', 'A'),
    RULE(MfCallsignEntityNetherlands, 4, 1, 0, 'P', 'C'),
    RULE(MfCallsignEntityNetherlands, 2, 7, 4, 'P', 'D'),
    RULE(MfCallsignEntityNetherlands, 2, 1, 3, 'P', 'E'),
    RULE(MfCallsignEntityNetherlands, 2, 0, 0, 'P', 'G'),
    RULE(MfCallsignEntityNetherlands, 0, 1, 2, 'P', 'I'),

    RULE(MfCallsignEntityAsiaticRussia, 41, 20, 0, 'R', 0),
    RULE(MfCallsignEntityAsiaticRussia, 45, 62, 38, 'R', MF_SET(MfSetAz)),
    RULE(MfCallsignEntityAsiaticRussia, 14, 18, 62, 'U', MF_SET(MfSetAi)),

    RULE(MfCallsignEntityArgentina, 5, 0, 0, 'L', 'T'),
    RULE(MfCallsignEntityArgentina, 2, 0, 0, 'L', 'V'),
    RULE(MfCallsignEntityArgentina, 2, 0, 0, 'A', 'Y'),
    RULE(MfCallsignEntityArgentina, 0, 6, 6, 'L', 'U'),
    RULE(MfCallsignEntityArgentina, 0, 1, 1, 'L', 'W'),

    RULE(MfCallsignEntitySlovenia, 1, 1, 1, 'S', '5'),

    RULE(MfCallsignEntityHungary, 2, 1, 1, 'H', 'G'),
    RULE(MfCallsignEntityHungary, 1, 19, 5, 'H', 'A'),
};

static char mf_pick_mask(MfRxRng* rng, uint32_t mask, uint8_t limit, char base) {
    uint8_t count = 0U;
    for(uint8_t bit = 0U; bit < limit; bit++)
        if(mask & (1UL << bit)) count++;
    uint32_t pick = mf_rx_rng_bounded(rng, count);
    for(uint8_t bit = 0U; bit < limit; bit++) {
        if((mask & (1UL << bit)) != 0U && pick-- == 0U) return (char)(base + bit);
    }
    return base;
}

static char mf_pick_spec(MfRxRng* rng, uint8_t spec) {
    if((spec & 0x80U) == 0U) return (char)spec;
    return mf_pick_mask(rng, mf_sets[spec & 0x7fU], 26U, 'A');
}

static char mf_pick_digit(MfRxRng* rng, uint8_t spec) {
    return mf_pick_mask(rng, mf_digit_masks[spec], 10U, '0');
}

static uint8_t mf_digit_spec(MfCallsignEntity entity, uint8_t len) {
    uint8_t spec = mf_entities[entity].lengths_digits >> 3U;
    if(spec == MfDigitsNonzeroAfterFour) return len == 4U ? MfDigitsAll : MfDigitsNonzero;
    if(spec == MfDigitsTwoToNineAfterFour) return len == 4U ? MfDigitsAll : MfDigitsTwoToNine;
    return spec;
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

static bool mf_spec_matches(uint8_t spec, char value) {
    if((spec & 0x80U) == 0U) return value == (char)spec;
    uint8_t bit = (uint8_t)(value - 'A');
    return bit < 26U && (mf_sets[spec & 0x7fU] & (1UL << bit)) != 0U;
}

static bool mf_rule_prefix_matches(const MfCallRule* rule, const MfCallsign* call) {
    uint8_t expected = rule->second == 0U ? 1U : 2U;
    return call->prefix_len == expected && mf_spec_matches(rule->first, call->prefix[0]) &&
           (expected == 1U || mf_spec_matches(rule->second, call->prefix[1]));
}

static MfCallsignEntity mf_pick_entity(MfRxRng* rng, uint8_t len) {
    uint8_t length_bit = (uint8_t)(1U << (len - 4U));
    uint16_t total = 0U;
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++)
        if((mf_entities[entity].lengths_digits & length_bit) != 0U)
            total += mf_entities[entity].weight;
    uint32_t roll = mf_rx_rng_bounded(rng, total);
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++) {
        if((mf_entities[entity].lengths_digits & length_bit) == 0U) continue;
        if(roll < mf_entities[entity].weight) return (MfCallsignEntity)entity;
        roll -= mf_entities[entity].weight;
    }
    return MfCallsignEntityUs;
}

static bool
    mf_callsign_construct(MfRxRng* rng, MfCallsignEntity entity, uint8_t len, MfCallsign* call) {
    uint8_t length_index = len - 4U;
    uint8_t start = mf_entity_rule_offsets[entity];
    uint8_t end = mf_entity_rule_offsets[entity + 1U];
    uint16_t total = 0U;
    for(uint8_t i = start; i < end; i++)
        total += mf_rules[i].weights[length_index];
    if(total == 0U) return false;
    uint32_t roll = mf_rx_rng_bounded(rng, total);
    const MfCallRule* rule = NULL;
    for(uint8_t i = start; i < end; i++) {
        uint8_t weight = mf_rules[i].weights[length_index];
        if(roll < weight) {
            rule = &mf_rules[i];
            break;
        }
        roll -= weight;
    }
    if(rule == NULL) return false;

    mf_call_clear(call, entity);
    call->prefix[call->prefix_len++] = mf_pick_spec(rng, rule->first);
    if(rule->second != 0U) call->prefix[call->prefix_len++] = mf_pick_spec(rng, rule->second);
    memcpy(call->text, call->prefix, call->prefix_len);
    call->text_len = call->prefix_len;
    call->text[call->text_len++] = mf_pick_digit(rng, mf_digit_spec(entity, len));
    while(call->text_len < len)
        call->text[call->text_len++] = (char)('A' + mf_rx_rng_bounded(rng, 26U));
    return mf_call_finish(call, len);
}

static bool mf_prefix_same(const MfCallsignGen* gen, const MfCallsign* call) {
    return gen->last_prefix_len == call->prefix_len && call->prefix_len != 0U &&
           memcmp(gen->last_prefix, call->prefix, call->prefix_len) == 0;
}

static bool mf_entity_valid(const MfCallsign* call, uint8_t target_len) {
    uint8_t length_index = target_len - 4U;
    uint8_t start = mf_entity_rule_offsets[call->entity];
    uint8_t end = mf_entity_rule_offsets[call->entity + 1U];
    uint8_t digit = (uint8_t)(call->text[call->prefix_len] - '0');
    uint16_t digit_mask = mf_digit_masks[mf_digit_spec(call->entity, target_len)];
    bool matched = false;
    for(uint8_t i = start; i < end; i++) {
        if(mf_rules[i].weights[length_index] != 0U && mf_rule_prefix_matches(&mf_rules[i], call) &&
           (digit_mask & (1U << digit)) != 0U) {
            matched = true;
            break;
        }
    }
    if(!matched) return false;
    if(call->entity == MfCallsignEntityItaly && call->prefix_len == 2U) {
        char second = call->prefix[1];
        if((second == 'T' && digit == 9U) || (second == 'S' && digit == 0U) ||
           ((second == 'M' || second == 'G' || second == 'P') && digit == 9U))
            return false;
    }
    if(call->entity == MfCallsignEntityArgentina && call->prefix_len == 2U &&
       call->prefix[0] == 'L' && call->prefix[1] == 'U' && digit == 1U &&
       call->text[call->prefix_len + 1U] == 'Z')
        return false;
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
    bool prefix_letter = false;
    if(call == NULL || target_len < 4U || target_len > MF_CALLSIGN_MAX_LEN ||
       call->entity >= MfCallsignEntityCount || call->text_len != target_len ||
       call->text[target_len] != '\0' || call->prefix_len == 0U ||
       call->prefix_len > MF_CALLSIGN_PREFIX_MAX || call->prefix_len >= target_len ||
       memcmp(call->text, call->prefix, call->prefix_len) != 0 ||
       call->text[call->prefix_len] < '0' || call->text[call->prefix_len] > '9')
        return false;
    for(uint8_t i = 0U; i < target_len; i++) {
        char ch = call->text[i];
        bool digit = ch >= '0' && ch <= '9';
        bool letter = ch >= 'A' && ch <= 'Z';
        if((!digit && !letter) || cw_symbol_count(cw(ch)) == 0U) return false;
        if(i < call->prefix_len && letter) prefix_letter = true;
        if(i > call->prefix_len && !letter) return false;
    }
    return prefix_letter && mf_entity_valid(call, target_len);
}

bool mf_callsign_generate(MfCallsignGen* gen, MfRxRng* rng, uint8_t target_len, MfCallsign* out) {
    MfCallsign candidate;
    if(gen == NULL || rng == NULL || out == NULL || target_len < 4U || target_len > 6U)
        return false;
    for(uint8_t attempt = 0U; attempt < 16U; attempt++) {
        MfCallsignEntity entity = mf_pick_entity(rng, target_len);
        if(mf_callsign_construct(rng, entity, target_len, &candidate) &&
           mf_callsign_valid(&candidate, target_len) && !mf_prefix_same(gen, &candidate)) {
            *out = candidate;
            memcpy(gen->last_prefix, candidate.prefix, candidate.prefix_len + 1U);
            gen->last_prefix_len = candidate.prefix_len;
            return true;
        }
    }
    for(uint8_t entity = 0U; entity < MfCallsignEntityCount; entity++) {
        if(mf_callsign_construct(rng, (MfCallsignEntity)entity, target_len, &candidate) &&
           mf_callsign_valid(&candidate, target_len) && !mf_prefix_same(gen, &candidate)) {
            *out = candidate;
            memcpy(gen->last_prefix, candidate.prefix, candidate.prefix_len + 1U);
            gen->last_prefix_len = candidate.prefix_len;
            return true;
        }
    }
    return false;
}
