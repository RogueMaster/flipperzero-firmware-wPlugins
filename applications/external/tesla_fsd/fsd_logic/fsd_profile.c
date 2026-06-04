#include "fsd_profile.h"
#include <string.h>

static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

// Case-insensitive prefix match: returns true if s starts with prefix.
static bool starts_ci(const char* s, const char* prefix) {
    while(*prefix) {
        if(lower(*s) != lower(*prefix)) return false;
        s++;
        prefix++;
    }
    return true;
}

// Read a base-10 number, advancing *pp past the digits.
static uint32_t read_uint(const char** pp) {
    const char* p = *pp;
    uint32_t v = 0;
    while(*p >= '0' && *p <= '9') {
        v = v * 10u + (uint32_t)(*p - '0');
        p++;
    }
    *pp = p;
    return v;
}

// Safety denylist — see header. Right stalk (gear / AP engage) must never be
// transmittable from a loadable text profile.
#define FSD_BLOCKED_RIGHT_STALK 0x229u

bool fsd_profile_id_blocked(uint32_t can_id) {
    return can_id == FSD_BLOCKED_RIGHT_STALK;
}

FsdProfileLineKind
    fsd_profile_parse_line(const char* line, FsdProfileStep* step, char* name_out, int name_cap) {
    while(*line == ' ' || *line == '\t')
        line++;
    if(*line == '\0' || *line == '\r' || *line == '\n') return FSD_PLINE_EMPTY;

    if(*line == '#') {
        const char* p = line + 1;
        while(*p == ' ' || *p == '\t')
            p++;
        if(starts_ci(p, "name:")) {
            p += 5;
            while(*p == ' ' || *p == '\t')
                p++;
            if(name_out && name_cap > 0) {
                int i = 0;
                while(p[i] && p[i] != '\r' && p[i] != '\n' && i < name_cap - 1) {
                    name_out[i] = p[i];
                    i++;
                }
                name_out[i] = '\0';
            }
            return FSD_PLINE_NAME;
        }
        return FSD_PLINE_EMPTY;
    }

    // Locate the ID#DATA token: find '#' with hex id digits before it. This
    // skips any leading "(ts) bus " candump prefix automatically.
    const char* hash = strchr(line, '#');
    if(!hash) return FSD_PLINE_ERROR;

    const char* idstart = hash;
    while(idstart > line && hexval(idstart[-1]) >= 0)
        idstart--;
    if(idstart == hash) return FSD_PLINE_ERROR; // no id digits before '#'

    int ndig = 0;
    uint32_t id = 0;
    for(const char* q = idstart; q < hash; q++) {
        id = (id << 4) | (uint32_t)hexval(*q);
        ndig++;
    }
    if(ndig > 8) return FSD_PLINE_ERROR;

    // Hex data byte pairs after '#'.
    const char* d = hash + 1;
    uint8_t data[MAX_LEN];
    int nb = 0;
    while(hexval(d[0]) >= 0 && hexval(d[1]) >= 0) {
        if(nb >= MAX_LEN) return FSD_PLINE_ERROR;
        data[nb++] = (uint8_t)((hexval(d[0]) << 4) | hexval(d[1]));
        d += 2;
    }
    if(hexval(d[0]) >= 0) return FSD_PLINE_ERROR; // dangling odd hex digit

    // Optional repeat=/delay= anywhere in the remainder.
    uint16_t repeat = 1, delay = 50;
    for(const char* r = d; *r; r++) {
        if(starts_ci(r, "repeat=")) {
            const char* p = r + 7;
            uint32_t v = read_uint(&p);
            repeat = (v == 0) ? 1 : (v > 0xFFFFu ? 0xFFFFu : (uint16_t)v);
            r = p - 1;
        } else if(starts_ci(r, "delay=")) {
            const char* p = r + 6;
            uint32_t v = read_uint(&p);
            delay = (v > 0xFFFFu) ? 0xFFFFu : (uint16_t)v;
            r = p - 1;
        }
    }

    step->can_id = id;
    step->dlc = (uint8_t)nb;
    step->repeat = repeat;
    step->delay_ms = delay;
    memset(step->data, 0, MAX_LEN);
    memcpy(step->data, data, (size_t)nb);
    // Safety: a denied id parses fine but must never reach the transmitter.
    if(fsd_profile_id_blocked(id)) return FSD_PLINE_BLOCKED;
    return FSD_PLINE_STEP;
}

bool fsd_profile_tx_allowed(const FSDState* state, uint32_t now_ms) {
    if(state->op_mode == OpMode_ListenOnly) return false; // never TX in listen
    if(!state->speed_seen) return false; // fail-closed: no speed proof
    if((now_ms - state->last_speed_tick_ms) > FSD_PROFILE_SPEED_FRESH_MS)
        return false; // speed frame is stale
    if(state->vehicle_speed_kph > 0.5f) return false; // must be stationary
    return true;
}
