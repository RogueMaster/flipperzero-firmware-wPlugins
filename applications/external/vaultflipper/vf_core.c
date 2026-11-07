// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#include "vf_core.h"

#include <stdlib.h>
#include <string.h>

#include "lib/monocypher/monocypher.h"

static const uint8_t vf_magic[4] = {'V', 'F', 'L', 'P'};
static const uint8_t vf_dir_magic[4] = {'V', 'F', 'D', '2'};
static const uint8_t vf_page_magic[4] = {'V', 'F', 'G', '2'};
static const char vf_x25519_label[] = "VaultFlipper x25519 v1";
static const char vf_empty[] = "";

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

void vf_wipe(void* p, size_t n) {
    if(p && n) crypto_wipe(p, n);
}

void vf_device_keypair(const uint8_t device_secret[32], uint8_t sk[32], uint8_t pk[32]) {
    crypto_blake2b_keyed(
        sk, 32, device_secret, 32, (const uint8_t*)vf_x25519_label, sizeof(vf_x25519_label) - 1);
    crypto_x25519_public_key(pk, sk);
}

VfResult vf_file_open(VfFile* f, VfReadFn read, void* ctx) {
    memset(f, 0, sizeof(*f));
    f->read = read;
    f->ctx = ctx;
    if(!read(ctx, 0, f->header, VF_HEADER_SIZE)) return VfErrIo;
    const uint8_t* h = f->header;
    if(memcmp(h, vf_magic, 4) != 0) return VfErrFormat;
    if(h[4] != VF_VERSION_BYTE || h[5] || h[6] || h[7]) return VfErrFormat;
    if(rd32(h + 68) != 0) return VfErrFormat;
    f->blocks = rd32(h + 8);
    f->passes = rd32(h + 12);
    f->dir_len = rd32(h + 64);
    if(f->blocks < VF_MIN_BLOCKS || f->blocks > VF_MAX_BLOCKS || (f->blocks % 4u) != 0)
        return VfErrParams; /* multiple of 4, as libsodium rounds */
    if(f->passes < VF_MIN_PASSES || f->passes > VF_MAX_PASSES) return VfErrParams;
    if(f->dir_len < 8 || f->dir_len > VF_MAX_DIR) return VfErrFormat;
    return VfOk;
}

static VfResult vf_derive_key(
    VfFile* f,
    const uint8_t dev_sk[32],
    const char* pin,
    size_t pin_len,
    void* given,
    size_t given_size) {
    if(pin_len == 0 || pin_len > VF_MAX_PIN) return VfErrAuth;

    uint8_t shared[32];
    crypto_x25519(shared, dev_sk, f->header + 32);
    uint8_t zero[32] = {0};
    if(crypto_verify32(shared, zero) == 0) {
        vf_wipe(shared, sizeof(shared));
        return VfErrFormat; /* low-order point: tampered file */
    }

    size_t work_size = (size_t)f->blocks * 1024u;
    void* work = (given && given_size >= work_size) ? given : malloc(work_size);
    if(!work) {
        vf_wipe(shared, sizeof(shared));
        return VfErrMemory;
    }
    uint8_t pin_key[32];
    crypto_argon2_config cfg = {
        .algorithm = CRYPTO_ARGON2_ID,
        .nb_blocks = f->blocks,
        .nb_passes = f->passes,
        .nb_lanes = 1,
    };
    crypto_argon2_inputs in = {
        .pass = (const uint8_t*)pin,
        .salt = f->header + 16,
        .pass_size = (uint32_t)pin_len,
        .salt_size = 16,
    };
    crypto_argon2(pin_key, sizeof(pin_key), work, cfg, in, crypto_argon2_no_extras);
    vf_wipe(work, work_size);
    if(work != given) free(work);

    crypto_blake2b_keyed(f->key, VF_KEY_SIZE, shared, sizeof(shared), pin_key, sizeof(pin_key));
    vf_wipe(shared, sizeof(shared));
    vf_wipe(pin_key, sizeof(pin_key));
    return VfOk;
}

/* Read and decrypt one block into a fresh buffer (caller frees). */
static VfResult
    vf_block_read(VfFile* f, uint32_t number, uint32_t offset, uint32_t length, uint8_t** out) {
    uint8_t nonce_tag[VF_BLOCK_OVERHEAD];
    if(!f->read(f->ctx, offset, nonce_tag, sizeof(nonce_tag))) return VfErrIo;
    uint8_t* buf = malloc(length ? length : 1);
    if(!buf) return VfErrMemory;
    if(length && !f->read(f->ctx, offset + VF_BLOCK_OVERHEAD, buf, length)) {
        free(buf);
        return VfErrIo;
    }
    uint8_t aad[VF_HEADER_SIZE + 4];
    memcpy(aad, f->header, VF_HEADER_SIZE);
    aad[VF_HEADER_SIZE + 0] = (uint8_t)number;
    aad[VF_HEADER_SIZE + 1] = (uint8_t)(number >> 8);
    aad[VF_HEADER_SIZE + 2] = (uint8_t)(number >> 16);
    aad[VF_HEADER_SIZE + 3] = (uint8_t)(number >> 24);
    if(crypto_aead_unlock(buf, nonce_tag + 24, f->key, nonce_tag, aad, sizeof(aad), buf, length) !=
       0) {
        vf_wipe(buf, length);
        free(buf);
        return VfErrAuth;
    }
    *out = buf;
    return VfOk;
}

static int vf_tag_to_field(uint8_t tag) {
    switch(tag) {
    case VfTagAccount:
        return VfFieldAccount;
    case VfTagGroup:
        return VfFieldGroup;
    case VfTagName:
        return VfFieldName;
    case VfTagUser:
        return VfFieldUser;
    case VfTagPass:
        return VfFieldPass;
    case VfTagTotp:
        return VfFieldTotp;
    case VfTagUri:
        return VfFieldUri;
    case VfTagMetaLayout:
        return VfFieldLayout;
    case VfTagMetaSynced:
        return VfFieldSynced;
    case VfTagFirst:
        return VfFieldFirst;
    case VfTagLast:
        return VfFieldLast;
    default:
        return -1; /* skip unknown tags (forward compatible) */
    }
}

static VfResult vf_parse_record(uint8_t* p, size_t len, size_t* pos, VfRecord* rec) {
    for(int i = 0; i < VfFieldCount; i++)
        rec->f[i] = vf_empty;
    if(*pos + 1 > len) return VfErrFormat;
    uint8_t nfields = p[(*pos)++];
    for(uint8_t i = 0; i < nfields; i++) {
        if(*pos + 3 > len) return VfErrFormat;
        uint8_t tag = p[*pos];
        uint16_t flen = rd16(p + *pos + 1);
        *pos += 3;
        if(*pos + flen + 1 > len) return VfErrFormat;
        if(p[*pos + flen] != 0) return VfErrFormat;
        if(memchr(p + *pos, 0, flen) != NULL) return VfErrFormat; /* no NUL inside a field */
        int field = vf_tag_to_field(tag);
        if(field >= 0) rec->f[field] = (const char*)(p + *pos);
        *pos += flen + 1;
    }
    return VfOk;
}

static VfResult vf_parse_dir(VfDir* d) {
    uint8_t* p = d->buf;
    size_t len = d->len;
    if(len < 7 || memcmp(p, vf_dir_magic, 4) != 0) return VfErrFormat;
    size_t pos = 4;
    VfResult r = vf_parse_record(p, len, &pos, &d->meta);
    if(r != VfOk) return r;

    if(pos + 1 > len) return VfErrFormat;
    uint8_t accounts = p[pos++];
    if(accounts == 0 || accounts > VF_MAX_ACCOUNTS) return VfErrFormat;
    for(uint8_t i = 0; i < accounts; i++) {
        VfRecord rec;
        r = vf_parse_record(p, len, &pos, &rec);
        if(r != VfOk) return r;
        d->accounts[i] = rec.f[VfFieldAccount];
    }
    d->account_count = accounts;

    if(pos + 2 > len) return VfErrFormat;
    uint16_t pages = rd16(p + pos);
    pos += 2;
    if(pages == 0 || pages > VF_MAX_PAGES) return VfErrFormat;
    d->pages = calloc(pages, sizeof(VfPageInfo));
    if(!d->pages) return VfErrMemory;
    for(uint16_t i = 0; i < pages; i++) {
        if(pos + 11 > len) return VfErrFormat;
        VfPageInfo* pi = &d->pages[i];
        pi->account = p[pos];
        pi->offset = rd32(p + pos + 1);
        pi->length = rd32(p + pos + 5);
        pi->count = rd16(p + pos + 9);
        pos += 11;
        bool codes = pi->account == VF_CODES_ACCOUNT;
        if((pi->account >= accounts && !codes) || pi->length > VF_MAX_PAGE || pi->count == 0)
            return VfErrFormat;
        VfRecord info;
        r = vf_parse_record(p, len, &pos, &info);
        if(r != VfOk) return r;
        pi->group = info.f[VfFieldGroup];
        pi->first = info.f[VfFieldFirst];
        pi->last = info.f[VfFieldLast];
        if(codes)
            d->code_count += pi->count;
        else
            d->entry_count += pi->count;
    }
    d->page_count = pages;
    return pos == len ? VfOk : VfErrFormat;
}

VfResult vf_file_unlock(
    VfFile* f,
    const uint8_t dev_sk[32],
    const char* pin,
    size_t pin_len,
    void* work,
    size_t work_size,
    VfDir* dir) {
    memset(dir, 0, sizeof(*dir));
    VfResult r = vf_derive_key(f, dev_sk, pin, pin_len, work, work_size);
    if(r != VfOk) return r;
    r = vf_block_read(f, 0, VF_HEADER_SIZE, f->dir_len, &dir->buf);
    if(r == VfOk) {
        dir->len = f->dir_len;
        f->unlocked = true;
        r = vf_parse_dir(dir);
    }
    if(r != VfOk) {
        vf_dir_free(dir);
        vf_file_close(f);
    }
    return r;
}

VfResult vf_page_load(VfFile* f, const VfDir* dir, uint16_t index, VfPage* out) {
    memset(out, 0, sizeof(*out));
    if(!f->unlocked || index >= dir->page_count) return VfErrFormat;
    const VfPageInfo* pi = &dir->pages[index];
    VfResult r = vf_block_read(f, (uint32_t)index + 1u, pi->offset, pi->length, &out->buf);
    if(r != VfOk) return r;
    out->len = pi->length;
    out->index = index;

    uint8_t* p = out->buf;
    size_t len = out->len;
    if(len < 6 || memcmp(p, vf_page_magic, 4) != 0 || rd16(p + 4) != pi->count) {
        vf_page_free(out);
        return VfErrFormat;
    }
    out->entries = calloc(pi->count, sizeof(VfRecord));
    if(!out->entries) {
        vf_page_free(out);
        return VfErrMemory;
    }
    size_t pos = 6;
    for(uint16_t i = 0; i < pi->count; i++) {
        r = vf_parse_record(p, len, &pos, &out->entries[i]);
        if(r != VfOk) {
            vf_page_free(out);
            return r;
        }
    }
    out->count = pi->count;
    return VfOk;
}

size_t vf_page_mem(const VfDir* dir, uint16_t index) {
    if(index >= dir->page_count) return 0;
    const VfPageInfo* pi = &dir->pages[index];
    return (size_t)pi->length + (size_t)pi->count * sizeof(VfRecord);
}

void vf_page_free(VfPage* p) {
    if(p->buf) {
        vf_wipe(p->buf, p->len);
        free(p->buf);
    }
    if(p->entries) {
        vf_wipe(p->entries, (size_t)p->count * sizeof(VfRecord));
        free(p->entries);
    }
    memset(p, 0, sizeof(*p));
}

void vf_dir_free(VfDir* d) {
    if(d->buf) {
        vf_wipe(d->buf, d->len);
        free(d->buf);
    }
    if(d->pages) {
        vf_wipe(d->pages, (size_t)d->page_count * sizeof(VfPageInfo));
        free(d->pages);
    }
    memset(d, 0, sizeof(*d));
}

void vf_file_close(VfFile* f) {
    vf_wipe(f->key, sizeof(f->key));
    f->unlocked = false;
}

void vf_fingerprint(const uint8_t pk[32], char out[10]) {
    static const char hex[] = "0123456789ABCDEF";
    uint8_t h[32];
    crypto_blake2b(h, sizeof(h), pk, 32);
    int o = 0;
    for(int i = 0; i < 4; i++) {
        if(i == 2) out[o++] = '-';
        out[o++] = hex[h[i] >> 4];
        out[o++] = hex[h[i] & 0x0F];
    }
    out[o] = 0;
}

static char vf_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool vf_contains(const char* haystack, const char* needle) {
    if(!needle[0]) return true;
    for(; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while(*h && *n && vf_lower(*h) == vf_lower(*n)) {
            h++;
            n++;
        }
        if(!*n) return true;
    }
    return false;
}
