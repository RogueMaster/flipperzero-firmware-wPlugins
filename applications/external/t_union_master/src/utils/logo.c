#include "logo.h"
#include "t_union_master_icons.h"

typedef struct {
    const char city_id[4 + 1];
    const Icon* logo;
} MtrLogo;

const MtrLogo mtr_logo_map[] = {
    {"1000", &I_logo_mtr_1000_32x32},
    {"1121", &I_logo_mtr_1121_32x32},
    {"2900", &I_logo_mtr_2900_32x32},
    {"3010", &I_logo_mtr_3010_32x32},
    {"5810", &I_logo_mtr_5810_32x32},
    {"5840", &I_logo_mtr_5840_32x32},
    {"6510", &I_logo_mtr_6510_32x32},
    {"6900", &I_logo_mtr_6900_32x32},
    {"7910", &I_logo_mtr_7910_32x32}};

const Icon* get_mtr_logo_by_city_id(const char* city_id) {
    for(uint8_t i = 0; i < COUNT_OF(mtr_logo_map); i++) {
        const MtrLogo* curr_logo = &mtr_logo_map[i];
        if(!strncmp(curr_logo->city_id, city_id, 4)) return curr_logo->logo;
    }
    return &I_logo_mtr_default_32x32;
}
