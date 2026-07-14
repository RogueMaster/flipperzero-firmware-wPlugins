#include "tracker_util.h"

#include <string.h>

// --- small helpers -------------------------------------------------------

static void trim_range(const char** ps, const char** pe) {
    const char* s = *ps;
    const char* e = *pe;
    while(s < e && (*s == ' ' || *s == '\t')) s++;
    while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    *ps = s;
    *pe = e;
}

static void copy_range(char* dst, size_t cap, const char* s, const char* e) {
    size_t len = (size_t)(e - s);
    if(len > cap - 1) len = cap - 1;
    memcpy(dst, s, len);
    dst[len] = '\0';
}

static bool key_is(const char* s, const char* e, const char* key) {
    size_t klen = strlen(key);
    if((size_t)(e - s) != klen) return false;
    for(size_t i = 0; i < klen; i++)
        if(s[i] != key[i]) return false;
    return true;
}

static bool starts(const char* p, const char* lit) {
    for(size_t i = 0; lit[i]; i++)
        if(p[i] != lit[i]) return false;
    return true;
}

// --- config --------------------------------------------------------------

bool config_parse(const char* buf, TrackerConfig* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    const char* p = buf;
    while(*p) {
        const char* ls = p;
        while(*p && *p != '\n') p++;
        const char* le = p;
        if(*p == '\n') p++;

        const char* s = ls;
        const char* e = le;
        trim_range(&s, &e);
        if(s == e || *s == '#') continue;

        const char* eq = s;
        while(eq < e && *eq != '=') eq++;
        if(eq == e) continue;

        const char* ks = s;
        const char* ke = eq;
        const char* vs = eq + 1;
        const char* ve = e;
        trim_range(&ks, &ke);
        trim_range(&vs, &ve);

        if(key_is(ks, ke, "WIFI_SSID")) {
            copy_range(cfg->wifi_ssid, sizeof(cfg->wifi_ssid), vs, ve);
            cfg->has_wifi = cfg->wifi_ssid[0] != '\0';
        } else if(key_is(ks, ke, "WIFI_PASS")) {
            copy_range(cfg->wifi_pass, sizeof(cfg->wifi_pass), vs, ve);
        } else if(key_is(ks, ke, "URL")) {
            copy_range(cfg->url, sizeof(cfg->url), vs, ve);
            cfg->has_url = cfg->url[0] != '\0';
        } else if(key_is(ks, ke, "HEADER")) {
            if(cfg->header_count < TU_HDR_MAX)
                copy_range(cfg->headers[cfg->header_count++], TU_HDR_LEN, vs, ve);
        } else if(key_is(ks, ke, "FIELD_STATUS")) {
            copy_range(cfg->field_status, TU_PATH_MAX, vs, ve);
        } else if(key_is(ks, ke, "FIELD_LOCATION")) {
            copy_range(cfg->field_location, TU_PATH_MAX, vs, ve);
        } else if(key_is(ks, ke, "FIELD_UPDATED")) {
            copy_range(cfg->field_updated, TU_PATH_MAX, vs, ve);
        }
    }
    return cfg->has_url;
}

// --- url templating ------------------------------------------------------

size_t url_build(
    const char* tmpl,
    const char* tracking,
    const char* carrier,
    char* out,
    size_t cap) {
    size_t o = 0;
    const char* p = tmpl;
    while(*p && o < cap - 1) {
        const char* rep = NULL;
        size_t skip = 0;
        if(*p == '{') {
            if(starts(p, "{tracking}")) {
                rep = tracking ? tracking : "";
                skip = 10;
            } else if(starts(p, "{carrier}")) {
                rep = carrier ? carrier : "";
                skip = 9;
            }
        }
        if(rep) {
            for(const char* r = rep; *r && o < cap - 1; r++) out[o++] = *r;
            p += skip;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    return o;
}

// --- json path extraction ------------------------------------------------

static const char* skip_ws(const char* p) {
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* skip_string(const char* p) {
    p++; // opening quote
    while(*p) {
        if(*p == '\\' && p[1]) {
            p += 2;
            continue;
        }
        if(*p == '"') return p + 1;
        p++;
    }
    return p;
}

static const char* skip_value(const char* p) {
    p = skip_ws(p);
    if(*p == '"') return skip_string(p);
    if(*p == '{' || *p == '[') {
        char open = *p;
        char close = (open == '{') ? '}' : ']';
        int depth = 0;
        while(*p) {
            if(*p == '"') {
                p = skip_string(p);
                continue;
            }
            if(*p == open)
                depth++;
            else if(*p == close) {
                depth--;
                if(depth == 0) return p + 1;
            }
            p++;
        }
        return p;
    }
    while(*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\n' &&
          *p != '\r')
        p++;
    return p;
}

static const char* obj_find(const char* p, const char* key, size_t keylen) {
    p = skip_ws(p);
    if(*p != '{') return NULL;
    p++;
    while(1) {
        p = skip_ws(p);
        if(*p == '}' || *p == '\0') return NULL;
        if(*p != '"') return NULL;
        const char* ks = p + 1;
        const char* nextp = skip_string(p);
        const char* ke = nextp - 1;
        bool match = ((size_t)(ke - ks) == keylen);
        if(match)
            for(size_t i = 0; i < keylen; i++)
                if(ks[i] != key[i]) {
                    match = false;
                    break;
                }
        p = skip_ws(nextp);
        if(*p != ':') return NULL;
        p++;
        p = skip_ws(p);
        if(match) return p;
        p = skip_value(p);
        p = skip_ws(p);
        if(*p == ',') {
            p++;
            continue;
        }
        return NULL;
    }
}

static const char* arr_nth(const char* p, int n) {
    p = skip_ws(p);
    if(*p != '[') return NULL;
    p++;
    p = skip_ws(p);
    if(*p == ']') return NULL;
    for(int i = 0; i < n; i++) {
        p = skip_value(p);
        p = skip_ws(p);
        if(*p != ',') return NULL;
        p++;
        p = skip_ws(p);
    }
    return p;
}

static bool seg_is_num(const char* s, const char* e) {
    if(s == e) return false;
    for(const char* p = s; p < e; p++)
        if(*p < '0' || *p > '9') return false;
    return true;
}

static int seg_to_int(const char* s, const char* e) {
    int v = 0;
    for(const char* p = s; p < e; p++) v = v * 10 + (*p - '0');
    return v;
}

static void extract_leaf(const char* p, char* out, size_t cap) {
    p = skip_ws(p);
    size_t o = 0;
    if(*p == '"') {
        p++;
        while(*p && *p != '"' && o < cap - 1) {
            if(*p == '\\' && p[1]) {
                char c = p[1];
                out[o++] = (c == 'n' || c == 't' || c == 'r') ? ' ' : c;
                p += 2;
                continue;
            }
            out[o++] = *p++;
        }
    } else {
        while(*p && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\t' &&
              *p != '\r' && o < cap - 1)
            out[o++] = *p++;
    }
    out[o] = '\0';
}

bool json_extract(const char* json, const char* path, char* out, size_t cap) {
    out[0] = '\0';
    if(!path || !*path) return false;
    const char* p = json;
    const char* seg = path;
    while(*seg) {
        const char* segend = seg;
        while(*segend && *segend != '.') segend++;
        p = skip_ws(p);
        if(seg_is_num(seg, segend) && *p == '[') {
            p = arr_nth(p, seg_to_int(seg, segend));
        } else if(*p == '{') {
            p = obj_find(p, seg, (size_t)(segend - seg));
        } else {
            return false;
        }
        if(!p) return false;
        seg = (*segend == '.') ? segend + 1 : segend;
    }
    extract_leaf(p, out, cap);
    return true;
}

// --- status mapping ------------------------------------------------------

static char lc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static bool contains_ci(const char* hay, const char* needle) {
    for(const char* h = hay; *h; h++) {
        const char* a = h;
        const char* b = needle;
        while(*a && *b && lc(*a) == lc(*b)) {
            a++;
            b++;
        }
        if(!*b) return true;
    }
    return false;
}

PackageStatus status_from_text(const char* s) {
    // Exception keywords win first (e.g. "Delivery Exception" is an exception).
    if(contains_ci(s, "except") || contains_ci(s, "fail") || contains_ci(s, "return"))
        return StatusException;
    if(contains_ci(s, "out for")) return StatusOutForDelivery;
    if(contains_ci(s, "deliver")) return StatusDelivered;
    if(contains_ci(s, "transit")) return StatusInTransit;
    return StatusPending;
}
