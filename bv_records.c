#include "bv_records.h"
#include "bv_vault.h"

#include <furi.h>
#include <string.h>

#define TAG "BioVaultRecords"

void bv_records_init(BvVaultData* v) {
    memset(v, 0, sizeof(*v));
}

void bv_records_clear(BvVaultData* v) {
    memset(v, 0, sizeof(*v));
}

// Copy src into a fixed-capacity NUL-terminated field. False if src too long.
static bool field_set(char* dst, size_t cap, const char* src) {
    size_t n = strlen(src);
    if(n >= cap) return false; // need room for the NUL
    memcpy(dst, src, n);
    dst[n] = '\0';
    return true;
}

bool bv_records_add(
    BvVaultData* v,
    BvEntryType type,
    const char* label,
    const char* user,
    const char* secret) {
    if(v->count >= BV_MAX_ENTRIES) return false;
    if(label == NULL || secret == NULL) return false;
    if(user == NULL) user = "";
    if(strlen(label) == 0) return false;

    BvEntry* e = &v->entries[v->count];
    memset(e, 0, sizeof(*e));
    e->type = (uint8_t)type;
    if(!field_set(e->label, BV_LABEL_CAP, label)) return false;
    if(!field_set(e->user, BV_USER_CAP, user)) return false;
    if(!field_set(e->secret, BV_SECRET_CAP, secret)) return false;
    v->count++;
    return true;
}

bool bv_records_set(
    BvVaultData* v,
    uint8_t index,
    BvEntryType type,
    const char* label,
    const char* user,
    const char* secret) {
    if(index >= v->count) return false;
    if(label == NULL || secret == NULL) return false;
    if(user == NULL) user = "";
    if(strlen(label) == 0) return false;

    // Validate into a temp first so a too-long field leaves the entry untouched.
    BvEntry tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.type = (uint8_t)type;
    if(!field_set(tmp.label, BV_LABEL_CAP, label)) return false;
    if(!field_set(tmp.user, BV_USER_CAP, user)) return false;
    if(!field_set(tmp.secret, BV_SECRET_CAP, secret)) return false;
    v->entries[index] = tmp;
    return true;
}

bool bv_records_remove(BvVaultData* v, uint8_t index) {
    if(index >= v->count) return false;
    for(uint8_t i = index; i + 1 < v->count; i++) {
        v->entries[i] = v->entries[i + 1];
    }
    v->count--;
    memset(&v->entries[v->count], 0, sizeof(BvEntry));
    return true;
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool ieq(const char* a, const char* b) {
    while(*a && *b) {
        if(lower(*a) != lower(*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

int bv_records_find(const BvVaultData* v, const char* label) {
    for(uint8_t i = 0; i < v->count; i++) {
        if(ieq(v->entries[i].label, label)) return i;
    }
    return -1;
}

// --- Serialization: [u8 count]{ [u8 type][u8 llen][label][u8 ulen][user][u8 slen][secret] } ---

static bool put_field(uint8_t* buf, size_t cap, size_t* pos, const char* s) {
    size_t n = strlen(s);
    if(n > 0xFF) return false;
    if(*pos + 1 + n > cap) return false;
    buf[(*pos)++] = (uint8_t)n;
    memcpy(buf + *pos, s, n);
    *pos += n;
    return true;
}

bool bv_records_serialize(const BvVaultData* v, uint8_t* buf, size_t cap, size_t* out_len) {
    if(cap < 1) return false;
    size_t pos = 0;
    buf[pos++] = v->count;
    for(uint8_t i = 0; i < v->count; i++) {
        const BvEntry* e = &v->entries[i];
        if(pos + 1 > cap) return false;
        buf[pos++] = e->type;
        if(!put_field(buf, cap, &pos, e->label)) return false;
        if(!put_field(buf, cap, &pos, e->user)) return false;
        if(!put_field(buf, cap, &pos, e->secret)) return false;
    }
    *out_len = pos;
    return true;
}

static bool get_field(const uint8_t* buf, size_t len, size_t* pos, char* dst, size_t cap) {
    if(*pos >= len) return false;
    uint8_t n = buf[(*pos)++];
    if(n >= cap) return false; // need room for NUL
    if(*pos + n > len) return false;
    memcpy(dst, buf + *pos, n);
    dst[n] = '\0';
    *pos += n;
    return true;
}

bool bv_records_parse(BvVaultData* v, const uint8_t* buf, size_t len) {
    bv_records_init(v);
    if(len < 1) return false;
    size_t pos = 0;
    uint8_t count = buf[pos++];
    if(count > BV_MAX_ENTRIES) return false;
    for(uint8_t i = 0; i < count; i++) {
        if(pos >= len) return false;
        BvEntry* e = &v->entries[i];
        memset(e, 0, sizeof(*e));
        e->type = buf[pos++];
        if(!get_field(buf, len, &pos, e->label, BV_LABEL_CAP)) return false;
        if(!get_field(buf, len, &pos, e->user, BV_USER_CAP)) return false;
        if(!get_field(buf, len, &pos, e->secret, BV_SECRET_CAP)) return false;
    }
    v->count = count;
    return true;
}

// --- Self-test ---

bool bv_records_selftest(void) {
    // BvVaultData is ~3.8 KB; keep it off the stack (heap only).
    BvVaultData* v = malloc(sizeof(BvVaultData));
    BvVaultData* v2 = malloc(sizeof(BvVaultData));

    bv_records_init(v);
    bool build_ok = bv_records_add(v, BvEntryCred, "example.com", "alice", "p@ss,w0rd\nwith,commas") &&
                    bv_records_add(v, BvEntryCred, "reddit.com", "bob", "swordfish") &&
                    bv_records_add(v, BvEntryNote, "recovery", "", "seed words go here") &&
                    (v->count == 3);

    // find (case-insensitive) + not-found
    bool idx_ok = (bv_records_find(v, "REDDIT.COM") == 1) && (bv_records_find(v, "nope") == -1);

    // serialize -> parse round-trip (v2 reused for the blob path below)
    uint8_t buf[512];
    size_t buf_len = 0;
    bool ser_ok = bv_records_serialize(v, buf, sizeof(buf), &buf_len) &&
                  bv_records_parse(v2, buf, buf_len) && (v2->count == 3) &&
                  (strcmp(v2->entries[0].secret, "p@ss,w0rd\nwith,commas") == 0) &&
                  (strcmp(v2->entries[2].label, "recovery") == 0);

    // full path: serialize -> seal to vault blob -> open -> parse
    BvVaultKey key;
    bool blob_ok = false;
    if(bv_vault_key_open(&key)) {
        uint8_t blob[600];
        size_t blob_len = 0;
        uint8_t out[512];
        size_t out_len = 0;
        blob_ok = (buf_len + BV_BLOB_OVERHEAD <= sizeof(blob)) &&
                  bv_vault_encrypt(&key, buf, buf_len, blob, &blob_len) &&
                  bv_vault_decrypt(&key, blob, blob_len, out, sizeof(out), &out_len) &&
                  bv_records_parse(v2, out, out_len) && (v2->count == 3) &&
                  (strcmp(v2->entries[0].user, "alice") == 0);
        bv_vault_key_clear(&key);
        memset(out, 0, sizeof(out));
    }

    bool ok = build_ok && idx_ok && ser_ok && blob_ok;
    FURI_LOG_I(
        TAG,
        "records self-test: %s (build=%d idx=%d ser=%d blob=%d, ser_len=%u)",
        ok ? "PASS" : "FAIL",
        build_ok,
        idx_ok,
        ser_ok,
        blob_ok,
        (unsigned)buf_len);

    bv_records_clear(v);
    bv_records_clear(v2);
    memset(buf, 0, sizeof(buf));
    free(v);
    free(v2);
    return ok;
}
