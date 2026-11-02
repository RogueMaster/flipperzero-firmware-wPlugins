#include "fn_life.h"

#include "fn_wire.h"

#include <stdlib.h>
#include <string.h>

#define FN_LIFE_DATE_32H_LEN 5u
#define FN_LIFE_DAYS_3BH_LEN 2u
#define FN_LIFE_MEMORY_3DH_LEN_MIN 8u
#define FN_LIFE_MEMORY_3DH_LEN_MARKING 9u

bool fn_parse_end_date_32h(const uint8_t* data, size_t len, FnLifeDate* out) {
    if(!data || !out || len != FN_LIFE_DATE_32H_LEN) return false;

    FnLifeDate date;
    date.end_date.year = data[0];
    date.end_date.month = data[1];
    date.end_date.day = data[2];
    date.reg_remaining = data[3];
    date.reg_done = data[4];

    *out = date;
    return true;
}

bool fn_parse_end_days_3bh(const uint8_t* data, size_t len, FnLifeDays* out) {
    if(!data || !out || len != FN_LIFE_DAYS_3BH_LEN) return false;

    FnLifeDays days;
    days.days = fn_wire_u16le_get(data);
    days.has_days = (days.days != 0);

    *out = days;
    return true;
}

bool fn_parse_memory_3dh(const uint8_t* data, size_t len, FnMemory* out) {
    if(!data || !out) return false;
    if(len != FN_LIFE_MEMORY_3DH_LEN_MIN && len != FN_LIFE_MEMORY_3DH_LEN_MARKING) {
        return false;
    }

    FnMemory memory;
    memset(&memory, 0, sizeof(memory));
    memory.five_year_documents = fn_wire_u32le_get(&data[0]);
    memory.thirty_day_kb = fn_wire_u32le_get(&data[4]);
    memory.has_marking = (len >= FN_LIFE_MEMORY_3DH_LEN_MARKING);
    if(memory.has_marking) {
        memory.marking_percent = data[8];
    }

    *out = memory;
    return true;
}

void fn_life_info_reset(FnLifeInfo* life) {
    if(!life) return;
    memset(life, 0, sizeof(FnLifeInfo));
}

FnLifeInfo* fn_life_info_alloc(void) {
    FnLifeInfo* life = malloc(sizeof(FnLifeInfo));
    if(life) fn_life_info_reset(life);
    return life;
}

void fn_life_info_free(FnLifeInfo* life) {
    free(life);
}
