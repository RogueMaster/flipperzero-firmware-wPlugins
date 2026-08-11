#include "meshcore_share_uri.h"

#include <stdio.h>
#include <string.h>

#define MESHCORE_CONTACT_PREFIX "meshcore://contact/add?"
#define MESHCORE_CHANNEL_PREFIX "meshcore://channel/add?"

/* One hex nibble, or -1. */
static int meshcore_hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode exactly `hex_len` hex chars from `hex` into `out`. hex_len must be
 * even and out must hold hex_len/2 bytes. Returns false on any non-hex char. */
static bool meshcore_hex_decode(const char* hex, size_t hex_len, uint8_t* out) {
    if(hex_len % 2 != 0) return false;
    for(size_t i = 0; i < hex_len; i += 2) {
        int hi = meshcore_hex_nibble(hex[i]);
        int lo = meshcore_hex_nibble(hex[i + 1]);
        if(hi < 0 || lo < 0) return false;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/* Length of the value starting at `p`, up to '&' or end of string. */
static size_t meshcore_value_len(const char* p) {
    size_t n = 0;
    while(p[n] != '\0' && p[n] != '&') n++;
    return n;
}

/* Find the first `key=value` in a `k1=v1&k2=v2` query and return the value
 * span. Returns false if the key is absent. Malformed tokens are skipped. */
static bool meshcore_query_value(
    const char* query,
    const char* key,
    const char** out_val,
    size_t* out_len) {
    size_t key_len = strlen(key);
    const char* p = query;
    while(*p != '\0') {
        const char* eq = strchr(p, '=');
        const char* amp = strchr(p, '&');
        /* A bare token with no '=' before the next '&' (or end) is skipped. */
        if(eq == NULL || (amp != NULL && eq > amp)) {
            if(amp == NULL) return false;
            p = amp + 1;
            continue;
        }
        size_t name_len = (size_t)(eq - p);
        const char* val = eq + 1;
        size_t val_len = meshcore_value_len(val);

        if(name_len == key_len && strncmp(p, key, key_len) == 0) {
            *out_val = val;
            *out_len = val_len;
            return true;
        }

        const char* next = val + val_len;
        if(*next == '\0') return false;
        p = next + 1; /* skip the '&' */
    }
    return false;
}

/* Percent/plus-decode `src` (len bytes) into `out` (cap bytes incl. NUL). A
 * '+' becomes a space and "%XX" becomes one byte; a stray '%' with bad hex is
 * copied literally. Always NUL-terminates. */
static void meshcore_url_decode(const char* src, size_t len, char* out, size_t cap) {
    if(cap == 0) return;
    size_t o = 0;
    for(size_t i = 0; i < len && o + 1 < cap; i++) {
        char c = src[i];
        if(c == '+') {
            out[o++] = ' ';
        } else if(c == '%' && i + 2 < len) {
            int hi = meshcore_hex_nibble(src[i + 1]);
            int lo = meshcore_hex_nibble(src[i + 2]);
            if(hi >= 0 && lo >= 0) {
                out[o++] = (char)((hi << 4) | lo);
                i += 2;
            } else {
                out[o++] = c;
            }
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
}

bool meshcore_contact_uri_parse(const char* uri, mc_contact_t* out) {
    if(uri == NULL || out == NULL) return false;

    const size_t prefix_len = strlen(MESHCORE_CONTACT_PREFIX);
    if(strncmp(uri, MESHCORE_CONTACT_PREFIX, prefix_len) != 0) return false;
    const char* query = uri + prefix_len;

    const char* val;
    size_t val_len;

    uint8_t key[32];
    if(!meshcore_query_value(query, "public_key", &val, &val_len)) return false;
    if(val_len != 64 || !meshcore_hex_decode(val, 64, key)) return false;

    if(!meshcore_query_value(query, "type", &val, &val_len) || val_len == 0) return false;
    long type = 0;
    for(size_t i = 0; i < val_len; i++) {
        if(val[i] < '0' || val[i] > '9') return false;
        type = type * 10 + (val[i] - '0');
    }
    if(type < 1 || type > 4) return false;

    char name[MC_NAME_LEN + 1] = {0};
    if(meshcore_query_value(query, "name", &val, &val_len)) {
        meshcore_url_decode(val, val_len, name, sizeof(name));
    }

    memset(out, 0, sizeof(*out));
    memcpy(out->public_key, key, sizeof(out->public_key));
    out->type = (uint8_t)type;
    /* 0xFF == flooded: no known path to a brand-new contact, so the node
     * rediscovers it on the first message, exactly as meshcore_py does. */
    out->out_path_len = 0xFF;
    snprintf(out->adv_name, sizeof(out->adv_name), "%s", name);

    return true;
}

bool meshcore_channel_uri_parse(
    const char* uri,
    char* name_out,
    size_t name_cap,
    uint8_t secret[MC_SECRET_LEN]) {
    if(uri == NULL || name_out == NULL || secret == NULL) return false;
    if(name_cap > 0) name_out[0] = '\0';
    memset(secret, 0, MC_SECRET_LEN);

    const size_t prefix_len = strlen(MESHCORE_CHANNEL_PREFIX);
    if(strncmp(uri, MESHCORE_CHANNEL_PREFIX, prefix_len) != 0) return false;
    const char* query = uri + prefix_len;

    const char* val;
    size_t val_len;

    if(!meshcore_query_value(query, "secret", &val, &val_len)) return false;
    if(val_len != MC_SECRET_LEN * 2 || !meshcore_hex_decode(val, MC_SECRET_LEN * 2, secret)) {
        memset(secret, 0, MC_SECRET_LEN);
        return false;
    }

    if(meshcore_query_value(query, "name", &val, &val_len)) {
        meshcore_url_decode(val, val_len, name_out, name_cap);
    }

    return true;
}
