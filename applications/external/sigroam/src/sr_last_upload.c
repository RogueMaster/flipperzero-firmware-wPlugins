#include "sr_last_upload.h"

#include <stdio.h>
#include <string.h>

static bool last_num(const char* text, size_t len, size_t* i, uint32_t* out) {
    size_t p;
    uint32_t v = 0u;
    bool any = false;

    if(text == NULL || i == NULL || out == NULL) {
        return false;
    }
    p = *i;
    while(p < len && text[p] >= '0' && text[p] <= '9') {
        unsigned d = (unsigned)(text[p] - '0');

        if(v > (UINT32_MAX - d) / 10u) {
            return false;
        }
        v = v * 10u + d;
        any = true;
        p++;
    }
    if(!any) {
        return false;
    }
    *i = p;
    *out = v;
    return true;
}

size_t sr_last_upload_format(const SrLastUpload* u, char* dst, size_t cap) {
    int n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(u == NULL || dst == NULL || cap == 0u) {
        return 0u;
    }
    n = snprintf(
        dst,
        cap,
        "v1 %u %u %u %u %lu %lu %u\n",
        (unsigned)u->month,
        (unsigned)u->day,
        (unsigned)u->hour,
        (unsigned)u->minute,
        (unsigned long)u->ok_n,
        (unsigned long)u->fail_n,
        u->wigle200 ? 1u : 0u);
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return 0u;
    }
    return (size_t)n;
}

SrLastParse sr_last_upload_parse(const char* text, size_t len, SrLastUpload* out) {
    size_t i = 3u;
    uint32_t field[7];
    unsigned k;
    SrLastUpload u;

    if(out == NULL || text == NULL || len < 3u || memcmp(text, "v1 ", 3) != 0) {
        return SrLastBad;
    }
    for(k = 0u; k < 7u; k++) {
        if(!last_num(text, len, &i, &field[k])) {
            return SrLastBad;
        }
        if(k + 1u < 7u) {
            if(i >= len || text[i] != ' ') {
                return SrLastBad;
            }
            i++;
        }
    }
    if(i < len && text[i] == '\n') {
        i++;
    }
    if(i != len) {
        return SrLastBad;
    }
    if(field[0] < 1u || field[0] > 12u || field[1] < 1u || field[1] > 31u || field[2] > 23u ||
       field[3] > 59u || field[6] > 1u) {
        return SrLastBad;
    }
    memset(&u, 0, sizeof(u));
    u.month = (uint8_t)field[0];
    u.day = (uint8_t)field[1];
    u.hour = (uint8_t)field[2];
    u.minute = (uint8_t)field[3];
    u.ok_n = field[4];
    u.fail_n = field[5];
    u.wigle200 = field[6] == 1u;
    *out = u;
    return SrLastOk;
}

size_t sr_last_upload_line(unsigned status, const SrLastUpload* u, char* dst, size_t cap) {
    int n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(dst == NULL || cap == 0u) {
        return 0u;
    }
    if(status == (unsigned)SrLastNone) {
        n = snprintf(dst, cap, "Last: none");
    } else if(status != (unsigned)SrLastOk || u == NULL) {
        n = snprintf(dst, cap, "Last: ?");
    } else if(u->fail_n > 0u) {
        n = snprintf(
            dst,
            cap,
            "Last: %02u-%02u %02u:%02u %lu fail",
            (unsigned)u->month,
            (unsigned)u->day,
            (unsigned)u->hour,
            (unsigned)u->minute,
            (unsigned long)u->fail_n);
    } else {
        n = snprintf(
            dst,
            cap,
            "Last: %02u-%02u %02u:%02u OK %lu",
            (unsigned)u->month,
            (unsigned)u->day,
            (unsigned)u->hour,
            (unsigned)u->minute,
            (unsigned long)u->ok_n);
    }
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return 0u;
    }
    return (size_t)n;
}

size_t sr_last_upload_pending(bool known, uint32_t nfiles, char* dst, size_t cap) {
    int n;

    if(dst != NULL && cap > 0u) {
        dst[0] = '\0';
    }
    if(dst == NULL || cap == 0u) {
        return 0u;
    }
    if(!known) {
        n = snprintf(dst, cap, "Not uploaded: ?");
    } else {
        n = snprintf(dst, cap, "Not uploaded: %lu logs", (unsigned long)nfiles);
    }
    if(n < 0 || (size_t)n >= cap) {
        dst[0] = '\0';
        return 0u;
    }
    return (size_t)n;
}
