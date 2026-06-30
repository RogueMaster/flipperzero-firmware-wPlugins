#ifndef T5577_CONFIG_H
#define T5577_CONFIG_H

#include <stdint.h>
#include <stdio.h>
#include <lib/lfrfid/tools/t5577.h>

#define MODULATION_NUM 11
#define CLOCK_NUM      8

typedef struct {
    char* modulation_name;
    uint32_t mod_page_zero;
} t5577_modulation;

extern const t5577_modulation all_mods[MODULATION_NUM];

typedef struct {
    uint8_t rf_clock_num;
    uint32_t clock_page_zero;
} t5577_rf_clock;

extern const t5577_rf_clock all_rf_clocks[CLOCK_NUM];

#endif // T5577_CONFIG_H
