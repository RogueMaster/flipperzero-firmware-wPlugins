/*
 * The keys/ directory on the SD card: which files are keys, what each one is,
 * names for new ones, and what deleting one does to the hosts. Pure C (no Furi); the
 * app lists the directory and reads file heads, this decides.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <proto/keyfmt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile.h"

#define KEYS_MAX     12 /* 12 x ~90 B, allocated while Keys or the host editor need it */
#define KEY_FILE_MAX PROTO_KEY_MAX /* bigger files are not keys */

typedef struct {
    char name[KEY_NAME_MAX + 1];
    uint8_t fmt; /* KeyFmt */
    uint8_t type; /* KeyType */
    uint16_t bits;
    bool encrypted;
    bool has_pub; /* <name>.pub exists */
    bool pub_known; /* fp is valid: from <name>.pub or the OpenSSH public section */
    char fp[KEYFMT_FP_SIZE];
    const char* problem; /* why it cannot be used (static text), or NULL */
} KeyEntry;

typedef struct {
    uint8_t count;
    KeyEntry e[KEYS_MAX];
} KeyList;

/* Directory entry filter: false for directories, *.pub, dotfiles, bad names, files
 * over KEY_FILE_MAX and empty files. */
bool keydir_is_candidate(const char* name, bool is_dir, uint64_t size);

/* Fill `e` from the first bytes of the file (`head`, up to KEYFMT_SNIFF_BYTES) and the
 * text of <name>.pub (NULL if none). `scratch` (KEYDIR_SCRATCH bytes) is wiped after. */
#define KEYDIR_SCRATCH \
    768u /* an RSA 4096 public section; bigger keys get their .pub at first use */
void keydir_inspect(
    KeyEntry* e,
    const char* name,
    const char* head,
    size_t head_len,
    const char* pub,
    size_t pub_len,
    uint8_t* scratch,
    size_t scratch_cap);

void keylist_init(KeyList* l);
/* Insert keeping names sorted (case-insensitive); false when full. */
bool keylist_add(KeyList* l, const KeyEntry* e);
int keylist_find(const KeyList* l, const char* name);

/* `base` if free, else base<sep>2, base<sep>3 ... ("id_ed25519_2", "flipper-1" with
 * first = 1). False if nothing fits. */
bool key_name_next(
    const KeyList* l,
    const char* base,
    char sep,
    unsigned first,
    char* out,
    size_t size);

/* "Ed25519", "ECDSA P-256", "RSA 3072", "RSA" ... */
void key_entry_label(const KeyEntry* e, char out[KEYFMT_LABEL_SIZE]);
/* Submenu row: "name - Ed25519", or "name (default)" for the default key. */
void key_row_label(const KeyEntry* e, bool is_default, char* out, size_t size);

/* Delete dialog text for the hosts that use the key: "Used by: nas, pi (+2)", or
 * "No host uses it." */
void key_users_text(const char* const* aliases, unsigned count, char* out, size_t size);

/* Does host `p` use key `name` (directly, or as the default key)? */
bool key_used_by(const HostProfile* p, const char* name, const char* default_key);
