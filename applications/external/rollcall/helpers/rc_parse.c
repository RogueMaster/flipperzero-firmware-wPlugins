#include "rc_parse.h"

#include <string.h>

/* Longest frame any built-in protocol produces is comfortably under this, so a
 * bigger number means we matched something that was not a bit count. */
#define RC_MAX_PLAUSIBLE_BITS 512

uint16_t rc_bits_from_dump(const char* dump) {
    if(!dump) return 0;

    for(const char* p = dump; (p = strstr(p, "bit")) != NULL; p += 3) {
        const char* d = p;
        while(d > dump && d[-1] >= '0' && d[-1] <= '9')
            d--;
        if(d == p) continue; // "bit" with no number in front of it

        uint32_t v = 0;
        for(const char* q = d; q < p; q++) {
            v = v * 10 + (uint32_t)(*q - '0');
            if(v > RC_MAX_PLAUSIBLE_BITS) break;
        }
        if(v > 0 && v <= RC_MAX_PLAUSIBLE_BITS) return (uint16_t)v;
    }

    return 0;
}
