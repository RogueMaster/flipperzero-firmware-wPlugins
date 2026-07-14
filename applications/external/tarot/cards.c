#include <gui/icon_i.h>
#include "cards.h"
#include <tarot_icons.h>
#include <stdlib.h>
#include <stddef.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>

// Major Arcana count (upright only)
const int card_number = 22;

const Card card[] = {
    // Major Arcana (upright)
    {"The Fool", &I_major_0},
    {"The Magician", &I_major_1},
    {"The High Priestess", &I_major_2},
    {"The Empress", &I_major_3},
    {"The Emperor", &I_major_4},
    {"The Hierophant", &I_major_5},
    {"The Lovers", &I_major_6},
    {"The Chariot", &I_major_7},
    {"Strength", &I_major_8},
    {"The Hermit", &I_major_9},
    {"Wheel of Fortune", &I_major_10},
    {"Justice", &I_major_11},
    {"The Hanged Man", &I_major_12},
    {"Death", &I_major_13},
    {"Temperance", &I_major_14},
    {"The Devil", &I_major_15},
    {"The Tower", &I_major_16},
    {"The Star", &I_major_17},
    {"The Moon", &I_major_18},
    {"The Sun", &I_major_19},
    {"Judgement", &I_major_20},
    {"The World", &I_major_21},
    // Major Arcana (reversed)
    {"The Fool", &I_major_0_},
    {"The Magician", &I_major_1_},
    {"The High Priestess", &I_major_2_},
    {"The Empress", &I_major_3_},
    {"The Emperor", &I_major_4_},
    {"The Hierophant", &I_major_5_},
    {"The Lovers", &I_major_6_},
    {"The Chariot", &I_major_7_},
    {"Strength", &I_major_8_},
    {"The Hermit", &I_major_9_},
    {"Wheel of Fortune", &I_major_10_},
    {"Justice", &I_major_11_},
    {"The Hanged Man", &I_major_12_},
    {"Death", &I_major_13_},
    {"Temperance", &I_major_14_},
    {"The Devil", &I_major_15_},
    {"The Tower", &I_major_16_},
    {"The Star", &I_major_17_},
    {"The Moon", &I_major_18_},
    {"The Sun", &I_major_19_},
    {"Judgement", &I_major_20_},
    {"The World", &I_major_21_},
    // Minor Arcana Placeholders (upright only, not used in spread)
    // Cups
    {"Ace of Cups", NULL},
    {"2 of Cups", NULL},
    {"3 of Cups", NULL},
    {"4 of Cups", NULL},
    {"5 of Cups", NULL},
    {"6 of Cups", NULL},
    {"7 of Cups", NULL},
    {"8 of Cups", NULL},
    {"9 of Cups", NULL},
    {"10 of Cups", NULL},
    {"Page of Cups", NULL},
    {"Knight of Cups", NULL},
    {"Queen of Cups", NULL},
    {"King of Cups", NULL},
    // Swords
    {"Ace of Swords", NULL},
    {"2 of Swords", NULL},
    {"3 of Swords", NULL},
    {"4 of Swords", NULL},
    {"5 of Swords", NULL},
    {"6 of Swords", NULL},
    {"7 of Swords", NULL},
    {"8 of Swords", NULL},
    {"9 of Swords", NULL},
    {"10 of Swords", NULL},
    {"Page of Swords", NULL},
    {"Knight of Swords", NULL},
    {"Queen of Swords", NULL},
    {"King of Swords", NULL},
    // Wands
    {"Ace of Wands", NULL},
    {"2 of Wands", NULL},
    {"3 of Wands", NULL},
    {"4 of Wands", NULL},
    {"5 of Wands", NULL},
    {"6 of Wands", NULL},
    {"7 of Wands", NULL},
    {"8 of Wands", NULL},
    {"9 of Wands", NULL},
    {"10 of Wands", NULL},
    {"Page of Wands", NULL},
    {"Knight of Wands", NULL},
    {"Queen of Wands", NULL},
    {"King of Wands", NULL},
    // Pentacles
    {"Ace of Pentacles", NULL},
    {"2 of Pentacles", NULL},
    {"3 of Pentacles", NULL},
    {"4 of Pentacles", NULL},
    {"5 of Pentacles", NULL},
    {"6 of Pentacles", NULL},
    {"7 of Pentacles", NULL},
    {"8 of Pentacles", NULL},
    {"9 of Pentacles", NULL},
    {"10 of Pentacles", NULL},
    {"Page of Pentacles", NULL},
    {"Knight of Pentacles", NULL},
    {"Queen of Pentacles", NULL},
    {"King of Pentacles", NULL}};

static uint32_t radio_noise_entropy() {
    furi_hal_subghz_reset();
    furi_hal_subghz_set_frequency(433920000); // ISM band
    furi_hal_subghz_rx();
    uint32_t entropy = 0;
    for(int i = 0; i < 32; ++i) {
        furi_delay_ms(2);
        float rssi = furi_hal_subghz_get_rssi();
        // Use the lowest bit of the mantissa for entropy
        uint32_t bit = ((uint32_t)(rssi * 1000)) & 0x01;
        entropy = (entropy << 1) ^ bit;
    }
    furi_hal_subghz_idle();
    return entropy;
}

uint16_t unbiased_rand(uint16_t max) {
    if(max == 0) return 0;
    uint32_t x, remainder = UINT32_MAX % max;
    do {
        x = furi_hal_random_get() ^ radio_noise_entropy();
    } while(x >= UINT32_MAX - remainder);
    return x % max;
}

int get_total_card_count(void) {
    return sizeof(card) / sizeof(card[0]);
}
