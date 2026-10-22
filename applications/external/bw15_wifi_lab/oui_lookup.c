#include "oui_lookup.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    uint32_t key;
    char name[G5_OUI_NAME];
} G5OuiEnt;

static G5OuiEnt g5_oui_tab[G5_OUI_MAX];
static uint16_t g5_oui_n;

void g5_oui_reset(void) {
    g5_oui_n = 0;
}

uint16_t g5_oui_count(void) {
    return g5_oui_n;
}

static int g5_hex(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Read exactly 6 hex digits, skipping ':' '-' '.'. Returns the pointer after
// the OUI, or NULL if the line does not start with an OUI.
static const char* g5_take_oui(const char* p, uint32_t* key_out) {
    uint32_t key = 0;
    int nibbles = 0;
    for(; *p && nibbles < 6; p++) {
        if(*p == ':' || *p == '-' || *p == '.') continue;
        int v = g5_hex(*p);
        if(v < 0) return NULL;
        key = (key << 4) | (uint32_t)v;
        nibbles++;
    }
    if(nibbles != 6) return NULL;
    *key_out = key;
    return p;
}

G5OuiAddResult g5_oui_add_line(const char* line) {
    if(!line) return G5OuiSkip;
    while(*line == ' ' || *line == '\t')
        line++;
    if(*line == '\0' || *line == '#') return G5OuiSkip;

    uint32_t key = 0;
    const char* p = g5_take_oui(line, &key);
    if(!p) return G5OuiSkip;

    // A full MAC ("AA:BB:CC:DD:EE:FF Vendor") keeps only the OUI.
    while(*p == ':' || *p == '-' || *p == '.')
        p++;
    if(g5_hex(*p) >= 0) {
        while(*p && *p != ' ' && *p != '\t')
            p++;
    }
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p == '\0' || *p == '#') return G5OuiSkip;

    char name[G5_OUI_NAME];
    memset(name, 0, sizeof(name));
    size_t n = 0;
    bool gap = false;
    for(; *p && *p != '#'; p++) {
        unsigned char c = (unsigned char)*p;
        if(c == ' ' || c == '\t') {
            if(n > 0) gap = true;
            continue;
        }
        if(c < 0x20 || c > 0x7E) continue;
        if(gap) {
            if(n + 1 >= G5_OUI_NAME) break;
            name[n++] = ' ';
            gap = false;
        }
        if(n + 1 >= G5_OUI_NAME) break;
        name[n++] = (char)c;
    }
    if(n == 0) return G5OuiSkip;
    name[n] = '\0';

    uint16_t lo = 0;
    uint16_t hi = g5_oui_n;
    while(lo < hi) {
        uint16_t mid = (uint16_t)(lo + ((hi - lo) / 2));
        if(g5_oui_tab[mid].key < key)
            lo = (uint16_t)(mid + 1);
        else
            hi = mid;
    }
    if(lo < g5_oui_n && g5_oui_tab[lo].key == key) {
        memcpy(g5_oui_tab[lo].name, name, G5_OUI_NAME);
        return G5OuiAdded;
    }
    if(g5_oui_n >= G5_OUI_MAX) return G5OuiFull;

    if(lo < g5_oui_n) {
        memmove(
            &g5_oui_tab[lo + 1], &g5_oui_tab[lo], (size_t)(g5_oui_n - lo) * sizeof(g5_oui_tab[0]));
    }
    g5_oui_tab[lo].key = key;
    memcpy(g5_oui_tab[lo].name, name, G5_OUI_NAME);
    g5_oui_n++;
    return G5OuiAdded;
}

const char* g5_oui_lookup(const char* mac) {
    if(!mac || g5_oui_n == 0) return NULL;
    uint32_t key = 0;
    int nibbles = 0;
    for(const char* p = mac; *p && nibbles < 6; p++) {
        if(*p == ':' || *p == '-' || *p == '.') continue;
        int v = g5_hex(*p);
        if(v < 0) return NULL;
        key = (key << 4) | (uint32_t)v;
        nibbles++;
    }
    if(nibbles < 6) return NULL;

    uint16_t lo = 0;
    uint16_t hi = g5_oui_n;
    while(lo < hi) {
        uint16_t mid = (uint16_t)(lo + ((hi - lo) / 2));
        if(g5_oui_tab[mid].key < key)
            lo = (uint16_t)(mid + 1);
        else if(g5_oui_tab[mid].key > key)
            hi = mid;
        else
            return g5_oui_tab[mid].name;
    }
    return NULL;
}
