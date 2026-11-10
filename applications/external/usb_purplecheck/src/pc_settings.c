// SPDX-License-Identifier: GPL-3.0
#include "pc_settings.h"
#include <string.h>
uint32_t pc_crc32(uint32_t previous, const uint8_t* data, size_t len) {
    uint32_t c = previous ^ 0xFFFFFFFFu;
    for(size_t i = 0; i < len; i++) {
        c ^= data[i];
        for(unsigned j = 0; j < 8; j++)
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0);
    }
    return c ^ 0xFFFFFFFFu;
}
void pc_settings_default(PcSettings* s) {
    s->language = PcEnglish;
    s->duration = 1;
    s->probe_files = true;
}
uint32_t pc_duration_ms(const PcSettings* s) {
    static const uint32_t values[] = {30000, 60000, 120000};
    return values[s->duration < 3 ? s->duration : 1];
}
void pc_settings_encode(const PcSettings* s, uint8_t out[PC_SETTINGS_BYTES]) {
    memset(out, 0, PC_SETTINGS_BYTES);
    memcpy(out, "PC03", 4);
    out[4] = (uint8_t)s->language;
    out[5] = s->duration;
    out[6] = s->probe_files ? 1 : 0;
    uint32_t c = pc_crc32(0, out, 8);
    for(unsigned i = 0; i < 4; i++)
        out[8 + i] = c >> (8 * i);
}
bool pc_settings_decode(PcSettings* s, const uint8_t* in, size_t len) {
    pc_settings_default(s);
    if(len != PC_SETTINGS_BYTES || memcmp(in, "PC03", 4) || in[4] >= PcLanguageCount ||
       in[5] >= 3 || in[6] > 1 || in[7])
        return false;
    uint32_t c = 0;
    for(unsigned i = 0; i < 4; i++)
        c |= (uint32_t)in[8 + i] << (8 * i);
    if(c != pc_crc32(0, in, 8)) return false;
    s->language = (PcLanguage)in[4];
    s->duration = in[5];
    s->probe_files = in[6] != 0;
    return true;
}
