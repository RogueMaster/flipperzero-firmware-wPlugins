#include "ihex.h"
#include <string.h>
#include <stdio.h>

static int hx(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int b2(const char* s, int i) {
    return (hx(s[i]) << 4) | hx(s[i + 1]);
}

int ihex_parse(const char* text, size_t len, uint8_t* img, uint32_t img_cap, uint32_t* out_len) {
    uint32_t base = 0, img_len = 0;
    size_t p = 0;
    memset(img, 0xFF, img_cap);
    while(p < len) {
        /* eine Zeile abgreifen */
        const char* line = &text[p];
        size_t q = p;
        while(q < len && text[q] != '\n' && text[q] != '\r')
            q++;
        size_t linelen = q - p;
        /* p auf Zeilenanfang der naechsten Zeile schieben */
        p = q;
        while(p < len && (text[p] == '\n' || text[p] == '\r'))
            p++;

        if(linelen < 11 || line[0] != ':') continue; /* :LLAAAATT..CC = min 11 Zeichen */
        int n = b2(line, 1);
        int addr = (b2(line, 3) << 8) | b2(line, 5);
        int type = b2(line, 7);
        if(n < 0 || addr < 0 || type < 0) continue;
        if(type == 0x01) break; /* EOF */
        if(type == 0x04) {
            base = ((uint32_t)((b2(line, 9) << 8) | b2(line, 11))) << 16;
            continue;
        }
        if(type != 0x00) continue;
        if((size_t)(9 + n * 2) > linelen) continue; /* Zeile zu kurz fuer n Bytes */
        for(int i = 0; i < n; i++) {
            uint32_t a = base + (uint32_t)addr + (uint32_t)i;
            if(a < img_cap) {
                img[a] = (uint8_t)b2(line, 9 + i * 2);
                if(a + 1 > img_len) img_len = a + 1;
            }
        }
    }
    if(out_len) *out_len = img_len;
    return 0;
}

int ihex_record(char* out, size_t cap, uint16_t addr, const uint8_t* data, uint8_t n) {
    int off = 0, sum = n + ((addr >> 8) & 0xFF) + (addr & 0xFF) + 0x00;
    if(cap < (size_t)(11 + n * 2 + 2)) return 0;
    off += snprintf(out + off, cap - off, ":%02X%04X00", n, addr);
    for(int i = 0; i < n; i++) {
        off += snprintf(out + off, cap - off, "%02X", data[i]);
        sum += data[i];
    }
    off += snprintf(out + off, cap - off, "%02X\r\n", (uint8_t)(-sum));
    return off;
}

int ihex_eof(char* out, size_t cap) {
    return snprintf(out, cap, ":00000001FF\r\n");
}
