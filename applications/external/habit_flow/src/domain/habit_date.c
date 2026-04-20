#include "include/domain/habit_date.h"

static int civil_to_days(int y, unsigned m, unsigned d) {
    y -= (int)(m <= 2u);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m > 2u ? m - 3u : m + 9u) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int)doe - 719468;
}

static void days_to_civil(int z, int *out_y, unsigned *out_m, unsigned *out_d) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    const int y_adj = (int)yoe + era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned d_out = doy - (153u * mp + 2u) / 5u + 1u;
    const unsigned m_out = mp < 10u ? mp + 3u : mp - 9u;
    *out_y = y_adj + (int)(m_out <= 2u);
    *out_m = m_out;
    *out_d = d_out;
}

uint32_t hf_date_pack(uint16_t year, uint8_t month, uint8_t day) {
    return (uint32_t)year * 10000u + (uint32_t)month * 100u + (uint32_t)day;
}

void hf_date_unpack(uint32_t packed, uint16_t *year, uint8_t *month, uint8_t *day) {
    *year = (uint16_t)(packed / 10000u);
    *month = (uint8_t)((packed / 100u) % 100u);
    *day = (uint8_t)(packed % 100u);
}

int32_t hf_date_diff_days(uint32_t from_packed, uint32_t to_packed) {
    uint16_t y1, y2;
    uint8_t m1, m2, d1, d2;
    hf_date_unpack(from_packed, &y1, &m1, &d1);
    hf_date_unpack(to_packed, &y2, &m2, &d2);
    const int a = civil_to_days((int)y1, m1, d1);
    const int b = civil_to_days((int)y2, m2, d2);
    return (int32_t)(b - a);
}

uint32_t hf_date_add_days(uint32_t packed, int32_t delta) {
    uint16_t y;
    uint8_t m, d;
    hf_date_unpack(packed, &y, &m, &d);
    int z = civil_to_days((int)y, m, d);
    z += (int)delta;
    int oy;
    unsigned om, od;
    days_to_civil(z, &oy, &om, &od);
    return hf_date_pack((uint16_t)oy, (uint8_t)om, (uint8_t)od);
}
