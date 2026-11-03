/* Text widths without a canvas (see fontw.h). SPDX-License-Identifier: GPL-3.0-only */
#include "fontw.h"
#include "fontw_table.h"

#include <stdio.h>
#include <string.h>

static int glyph_index(char c) {
    unsigned u = (unsigned char)c;
    return (u < 32 || u > 126) ? -1 : (int)(u - 32);
}

uint8_t fontw_secondary_glyph(void* ctx, char c) {
    (void)ctx;
    int i = glyph_index(c);
    return i < 0 ? 0 : fontw_secondary_adv[i];
}

uint8_t fontw_mono_glyph(void* ctx, char c) {
    (void)ctx;
    (void)c;
    return FONTW_MONO;
}

/* u8g2_GetStrWidth: advances, but the last glyph counts with its box */
uint16_t fontw_secondary(const char* s) {
    int w = 0, last = -1;
    for(; *s; s++) {
        int i = glyph_index(*s);
        if(i < 0) continue;
        w += fontw_secondary_adv[i];
        last = i;
    }
    if(last >= 0 && fontw_secondary_last[last])
        w = w - fontw_secondary_adv[last] + fontw_secondary_last[last];
    return (uint16_t)(w < 0 ? 0 : w);
}

void fontw_fit_end(char* s, size_t size, uint16_t max_px) {
    if(!size || fontw_secondary(s) <= max_px) return;
    char buf[96];
    size_t n = strlen(s);
    if(n > sizeof(buf) - 4) n = sizeof(buf) - 4;
    while(n > 0) {
        size_t k = n;
        while(k > 0 && s[k - 1] == ' ')
            k--; /* no space right before the dots */
        memcpy(buf, s, k);
        memcpy(buf + k, "...", 4);
        if(fontw_secondary(buf) <= max_px) break;
        n--;
    }
    if(!n) memcpy(buf, "...", 4);
    strncpy(s, buf, size - 1);
    s[size - 1] = '\0';
}

void fontw_fit_tail(char* s, size_t size, uint16_t max_px) {
    if(!size || fontw_secondary(s) <= max_px) return;
    char buf[96];
    size_t len = strlen(s);
    const char* p = s + (len > sizeof(buf) - 4 ? len - (sizeof(buf) - 4) : 0);
    for(; *p; p++) {
        if(*p == ' ' || *p == '.') continue; /* no space or second dot run after "..." */
        snprintf(buf, sizeof(buf), "...%s", p);
        if(fontw_secondary(buf) <= max_px) break;
    }
    if(!*p) memcpy(buf, "...", 4);
    strncpy(s, buf, size - 1);
    s[size - 1] = '\0';
}
