#include "ck42x_vault_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ck_vault_copy(char* dst, size_t dst_size, const char* src) {
    if(dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void ck_vault_secure_zero(void* ptr, size_t len) {
    volatile uint8_t* p = ptr;
    while(len--)
        *p++ = 0;
}

void ck_vault_store_init(CkVaultStore* store) {
    if(!store) return;
    store->entries = NULL;
    store->count = 0;
    store->capacity = 0;
}

void ck_vault_store_reset(CkVaultStore* store) {
    if(!store) return;
    if(store->entries && store->count) {
        ck_vault_secure_zero(store->entries, store->count * sizeof(CkVaultEntry));
    }
    store->count = 0;
}

void ck_vault_store_free(CkVaultStore* store) {
    if(!store) return;
    if(store->entries) {
        ck_vault_secure_zero(store->entries, store->capacity * sizeof(CkVaultEntry));
        free(store->entries);
    }
    ck_vault_store_init(store);
}

bool ck_vault_store_reserve(CkVaultStore* store, size_t needed) {
    if(!store) return false;
    if(needed <= store->capacity) return true;
    if(needed > SIZE_MAX / sizeof(CkVaultEntry)) return false;

    size_t cap = store->capacity ? store->capacity : 8;
    while(cap < needed) {
        if(cap > (SIZE_MAX / 2) / sizeof(CkVaultEntry)) {
            cap = needed;
            break;
        }
        cap *= 2;
    }
    if(cap < needed) cap = needed;
    if(cap > SIZE_MAX / sizeof(CkVaultEntry)) return false;

    CkVaultEntry* next = malloc(cap * sizeof(CkVaultEntry));
    if(!next) return false;
    memset(next, 0, cap * sizeof(CkVaultEntry));
    if(store->entries && store->count) {
        memcpy(next, store->entries, store->count * sizeof(CkVaultEntry));
        ck_vault_secure_zero(store->entries, store->capacity * sizeof(CkVaultEntry));
    }
    free(store->entries);
    store->entries = next;
    store->capacity = cap;
    return true;
}

bool ck_vault_store_append(CkVaultStore* store, const CkVaultEntry* entry) {
    if(!store || !entry) return false;
    if(store->count == SIZE_MAX) return false;
    if(!ck_vault_store_reserve(store, store->count + 1)) return false;
    store->entries[store->count++] = *entry;
    return true;
}

static bool ck_vault_line_has_two_tabs(const char* line, const char* line_end) {
    int tabs = 0;
    for(const char* p = line; p < line_end; p++) {
        if(*p == '\t') tabs++;
    }
    return tabs >= 2;
}

static size_t ck_vault_count_entries(const char* buf) {
    size_t count = 0;
    const char* line = buf;
    while(line && *line) {
        const char* next = strchr(line, '\n');
        const char* line_end = next ? next : line + strlen(line);
        if(ck_vault_line_has_two_tabs(line, line_end)) count++;
        line = next ? next + 1 : NULL;
    }
    return count;
}

bool ck_vault_store_parse(CkVaultStore* store, char* buf) {
    if(!store || !buf) return false;
    ck_vault_store_reset(store);

    size_t needed = ck_vault_count_entries(buf);
    if(needed && !ck_vault_store_reserve(store, needed)) return false;

    char* line = buf;
    while(line && *line) {
        char* next = strchr(line, '\n');
        if(next) {
            *next = '\0';
            next++;
        }
        char* tab1 = strchr(line, '\t');
        if(tab1) {
            *tab1 = '\0';
            char* tab2 = strchr(tab1 + 1, '\t');
            if(tab2) {
                *tab2 = '\0';
                if(!ck_vault_store_reserve(store, store->count + 1)) {
                    ck_vault_store_reset(store);
                    return false;
                }
                CkVaultEntry* e = &store->entries[store->count++];
                ck_vault_copy(e->account, sizeof(e->account), line);
                ck_vault_copy(e->username, sizeof(e->username), tab1 + 1);
                ck_vault_copy(e->password, sizeof(e->password), tab2 + 1);
            }
        }
        line = next;
    }
    return true;
}

bool ck_vault_store_serialize(const CkVaultStore* store, uint8_t** out, size_t* out_len) {
    if(!store || !out || !out_len) return false;

    size_t cap = 1;
    if(store->count) {
        if(store->count > (SIZE_MAX - 1) / CK_LINE_MAX) return false;
        cap = store->count * CK_LINE_MAX + 1;
    }

    char* buf = malloc(cap);
    if(!buf) return false;

    size_t used = 0;
    buf[0] = '\0';
    for(size_t i = 0; i < store->count; i++) {
        int len = snprintf(
            buf + used,
            cap - used,
            "%s\t%s\t%s\n",
            store->entries[i].account,
            store->entries[i].username,
            store->entries[i].password);
        if(len <= 0 || (size_t)len >= cap - used) {
            ck_vault_secure_zero(buf, cap);
            free(buf);
            return false;
        }
        used += (size_t)len;
    }

    *out = (uint8_t*)buf;
    *out_len = used;
    return true;
}
