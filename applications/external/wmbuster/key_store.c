#include "key_store.h"
#include "wmbus_app_i.h"
#include "protocol/wmbus_manuf.h"
#include <stdio.h>
#include <string.h>

#define KEY_STORE_MAX 32

typedef struct {
    uint16_t m;
    uint32_t id;
    uint8_t  key[16];
    bool     used;
} Entry;

struct KeyStore {
    Storage* storage;
    Entry    entries[KEY_STORE_MAX];
};

KeyStore* key_store_alloc(Storage* s) {
    KeyStore* ks = malloc(sizeof(KeyStore));
    memset(ks, 0, sizeof(*ks));
    ks->storage = s;
    return ks;
}

void key_store_free(KeyStore* ks) { free(ks); }

static int hex_nyb(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return 10 + c - 'a';
    if(c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static bool parse_hex(const char* s, uint8_t* out, size_t n) {
    for(size_t i = 0; i < n; i++) {
        int hi = hex_nyb(s[2*i]); int lo = hex_nyb(s[2*i+1]);
        if(hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static uint16_t encode_manuf(const char* c) {
    return (uint16_t)((((c[0] - 64) & 0x1F) << 10) |
                      (((c[1] - 64) & 0x1F) << 5)  |
                      (((c[2] - 64) & 0x1F)));
}

bool key_store_load(KeyStore* ks) {
    storage_simply_mkdir(ks->storage, WMBUS_APP_DATA_DIR);
    File* f = storage_file_alloc(ks->storage);
    bool ok = storage_file_open(f, WMBUS_APP_KEYS_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!ok) { storage_file_free(f); return true; /* no file yet — fine */ }
    char line[128];
    size_t pos = 0;
    while(true) {
        uint8_t b;
        uint16_t r = storage_file_read(f, &b, 1);
        if(r == 0) { line[pos] = 0; }
        if(r == 0 || b == '\n') {
            line[pos] = 0;
            if(pos > 5 && line[0] != '#') {
                /* MAN,IDHEX,KEYHEX */
                char* c1 = strchr(line, ',');
                if(c1) {
                    *c1++ = 0;
                    char* c2 = strchr(c1, ',');
                    if(c2 && (size_t)(c2 - c1) == 8) {
                        *c2++ = 0;
                        if(strlen(line) >= 3 && strlen(c2) >= 32) {
                            uint16_t m = encode_manuf(line);
                            uint32_t id = 0;
                            for(int i = 0; i < 8; i++) {
                                int v = hex_nyb(c1[i]); if(v < 0) { id = 0; break; }
                                id = (id << 4) | (uint32_t)v;
                            }
                            uint8_t k[16];
                            if(parse_hex(c2, k, 16))
                                key_store_set(ks, m, id, k);
                        }
                    }
                }
            }
            pos = 0;
            if(r == 0) break;
            continue;
        }
        if(b == '\r') continue;
        if(pos < sizeof(line) - 1) line[pos++] = (char)b;
    }
    storage_file_close(f); storage_file_free(f);
    return true;
}

bool key_store_save(KeyStore* ks) {
    storage_simply_mkdir(ks->storage, WMBUS_APP_DATA_DIR);
    File* f = storage_file_alloc(ks->storage);
    bool ok = storage_file_open(f, WMBUS_APP_KEYS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!ok) { storage_file_free(f); return false; }
    const char* hdr = "# wM-Bus Inspector keys: MAN,ID,KEY (32 hex)\n";
    storage_file_write(f, hdr, strlen(hdr));
    for(int i = 0; i < KEY_STORE_MAX; i++) {
        if(!ks->entries[i].used) continue;
        char m[4]; wmbus_manuf_decode(ks->entries[i].m, m);
        char buf[80];
        int n = snprintf(buf, sizeof(buf), "%s,%08lX,", m,
                         (unsigned long)ks->entries[i].id);
        storage_file_write(f, buf, (uint16_t)n);
        for(int k = 0; k < 16; k++) {
            char hx[3]; snprintf(hx, sizeof(hx), "%02X", ks->entries[i].key[k]);
            storage_file_write(f, hx, 2);
        }
        storage_file_write(f, "\n", 1);
    }
    storage_file_close(f); storage_file_free(f);
    return true;
}

bool key_store_lookup(KeyStore* ks, uint16_t m, uint32_t id, uint8_t out[16]) {
    for(int i = 0; i < KEY_STORE_MAX; i++)
        if(ks->entries[i].used && ks->entries[i].m == m && ks->entries[i].id == id) {
            memcpy(out, ks->entries[i].key, 16); return true;
        }
    return false;
}

bool key_store_set(KeyStore* ks, uint16_t m, uint32_t id, const uint8_t k[16]) {
    int free_slot = -1;
    for(int i = 0; i < KEY_STORE_MAX; i++) {
        if(ks->entries[i].used && ks->entries[i].m == m && ks->entries[i].id == id) {
            memcpy(ks->entries[i].key, k, 16); return true;
        }
        if(!ks->entries[i].used && free_slot < 0) free_slot = i;
    }
    if(free_slot < 0) return false;
    ks->entries[free_slot].used = true;
    ks->entries[free_slot].m = m;
    ks->entries[free_slot].id = id;
    memcpy(ks->entries[free_slot].key, k, 16);
    return true;
}

void key_store_iter(KeyStore* ks, KeyStoreIterCb cb, void* ctx) {
    for(int i = 0; i < KEY_STORE_MAX; i++)
        if(ks->entries[i].used)
            cb(ks->entries[i].m, ks->entries[i].id, ks->entries[i].key, ctx);
}
