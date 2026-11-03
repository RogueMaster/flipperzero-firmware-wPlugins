/* keys/ directory logic (see keydir.h). SPDX-License-Identifier: GPL-3.0-only */
#include "keydir.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

bool keydir_is_candidate(const char* name, bool is_dir, uint64_t size) {
    return !is_dir && size > 0 && size <= KEY_FILE_MAX && key_name_valid(name);
}

void keydir_inspect(
    KeyEntry* e,
    const char* name,
    const char* head,
    size_t head_len,
    const char* pub,
    size_t pub_len,
    uint8_t* scratch,
    size_t scratch_cap) {
    memset(e, 0, sizeof(*e));
    snprintf(e->name, sizeof(e->name), "%s", name);
    KeyInfo ki;
    keyfmt_sniff(
        head,
        head_len < KEYFMT_SNIFF_BYTES ? head_len : KEYFMT_SNIFF_BYTES,
        &ki,
        scratch,
        scratch_cap);
    e->fmt = (uint8_t)ki.fmt;
    e->type = (uint8_t)ki.type;
    e->bits = ki.bits;
    e->encrypted = ki.encrypted;
    if(ki.pub) {
        keyfmt_fingerprint(ki.pub, ki.pub_len, e->fp);
        e->pub_known = true;
    }
    e->problem = keyfmt_problem(&ki);
    if(pub && pub_len) {
        /* the .pub tells type and size even for an encrypted PEM key */
        e->has_pub = true;
        size_t blob_len = 0;
        if(keyfmt_parse_pub_line(pub, pub_len, scratch, scratch_cap, &blob_len)) {
            KeyInfo pi = {.fmt = KeyFmtOpenSsh};
            if(keyfmt_blob_info(scratch, blob_len, &pi)) {
                char fp[KEYFMT_FP_SIZE];
                keyfmt_fingerprint(scratch, blob_len, fp);
                if(!e->pub_known) {
                    memcpy(e->fp, fp, sizeof(fp));
                    e->pub_known = true;
                }
                /* a .pub that belongs to another key type is not trusted for it */
                if(e->type == KeyTypeUnknown || (e->type == pi.type && !e->bits)) {
                    e->type = (uint8_t)pi.type;
                    e->bits = pi.bits;
                }
            }
        }
    }
    memset(scratch, 0, scratch_cap);
}

void keylist_init(KeyList* l) {
    memset(l, 0, sizeof(*l));
}

bool keylist_add(KeyList* l, const KeyEntry* e) {
    if(l->count >= KEYS_MAX) return false;
    unsigned i = l->count;
    while(i > 0 && strcasecmp(l->e[i - 1].name, e->name) > 0) {
        l->e[i] = l->e[i - 1];
        i--;
    }
    l->e[i] = *e;
    l->count++;
    return true;
}

int keylist_find(const KeyList* l, const char* name) {
    for(unsigned i = 0; i < l->count; i++)
        if(strcmp(l->e[i].name, name) == 0) return (int)i;
    return -1;
}

bool key_name_next(
    const KeyList* l,
    const char* base,
    char sep,
    unsigned first,
    char* out,
    size_t size) {
    if(keylist_find(l, base) < 0 && key_name_valid(base)) {
        snprintf(out, size, "%s", base);
        return true;
    }
    for(unsigned k = first; k < first + KEYS_MAX + 1; k++) {
        snprintf(out, size, "%s%c%u", base, sep, k);
        if(keylist_find(l, out) < 0 && key_name_valid(out)) return true;
    }
    return false;
}

void key_entry_label(const KeyEntry* e, char out[KEYFMT_LABEL_SIZE]) {
    KeyInfo li = {.type = (KeyType)e->type, .bits = e->bits};
    keyfmt_label(&li, out);
}

void key_row_label(const KeyEntry* e, bool is_default, char* out, size_t size) {
    /* the name and what sets it apart; the type is on the key's card */
    if(e->problem)
        snprintf(out, size, "%s (not usable)", e->name);
    else if(is_default)
        snprintf(out, size, "%s (default)", e->name);
    else
        snprintf(out, size, "%s", e->name);
}

void key_users_text(const char* const* aliases, unsigned count, char* out, size_t size) {
    if(count == 0) {
        snprintf(out, size, "No host uses it.");
        return;
    }
    size_t n = (size_t)snprintf(out, size, "Used by: ");
    unsigned shown = count > 2 ? 2 : count;
    for(unsigned i = 0; i < shown && n < size; i++)
        n += (size_t)snprintf(out + n, size - n, "%s%s", i ? ", " : "", aliases[i]);
    if(count > shown && n < size) snprintf(out + n, size - n, " (+%u)", count - shown);
}

bool key_used_by(const HostProfile* p, const char* name, const char* default_key) {
    const char* k = profile_key_name(p, default_key);
    return k && strcmp(k, name) == 0;
}
