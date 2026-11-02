/* keys/ directory logic and the per-host key profile fields. SPDX-License-Identifier: GPL-3.0-only
 *
 * No private key is committed: the "files" here are public key lines (from keys that
 * were deleted after ssh-keygen -lf) and synthetic headers. */
#include "keydir.h"
#include "profile.h"
#include "tt.h"

#include <stdio.h>
#include <string.h>

static uint8_t scratch[KEYDIR_SCRATCH];

static const char ED_PUB[] =
    "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIKmbNVUx4KjFZz5Wy6q9UbOAJmjohi5fktxyWWAjI2cl fixture\n";
static const char ED_FP[] = "SHA256:hNUjotZMZK2WpL3WDqUZZeUsLhngno+bZJ8NIDO+9sA";

static void test_names(void) {
    TT_CHECK(key_name_valid("id_ed25519"));
    TT_CHECK(key_name_valid("work.key-2"));
    TT_CHECK(!key_name_valid(""));
    TT_CHECK(!key_name_valid(".hidden"));
    TT_CHECK(!key_name_valid("id_rsa.pub"));
    TT_CHECK(!key_name_valid("a/b"));
    TT_CHECK(!key_name_valid("with space"));
    TT_CHECK(!key_name_valid("abcdefghijklmnopqrstuvwxyz0123456")); /* 33 chars */
    TT_CHECK(key_name_valid("abcdefghijklmnopqrstuvwxyz012345"));

    TT_CHECK(keydir_is_candidate("id_rsa", false, 2600));
    TT_CHECK(!keydir_is_candidate("id_rsa.pub", false, 600));
    TT_CHECK(!keydir_is_candidate("old", true, 0));
    TT_CHECK(!keydir_is_candidate(".DS_Store", false, 6000));
    TT_CHECK(!keydir_is_candidate("big", false, KEY_FILE_MAX + 1));
    TT_CHECK(!keydir_is_candidate("empty", false, 0));
}

static void test_inspect(void) {
    KeyEntry e;
    /* encrypted PEM: type from the header, size and fingerprint only from the .pub */
    static const char pem[] =
        "-----BEGIN RSA PRIVATE KEY-----\nProc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00\n\nAAAA\n";
    keydir_inspect(&e, "old_rsa", pem, sizeof(pem) - 1, NULL, 0, scratch, sizeof(scratch));
    TT_CHECK(e.encrypted);
    TT_EQ_INT(e.type, KeyTypeRsa);
    TT_CHECK(!e.pub_known);
    char l[KEYFMT_LABEL_SIZE];
    key_entry_label(&e, l);
    TT_EQ_STR(l, "RSA");
    keydir_inspect(&e, "old_rsa", pem, sizeof(pem) - 1, NULL, 0, scratch, sizeof(scratch));
    TT_CHECK(e.problem == NULL);

    /* a mismatched .pub (an ed25519 line next to an RSA key) does not change the type */
    keydir_inspect(
        &e, "old_rsa", pem, sizeof(pem) - 1, ED_PUB, sizeof(ED_PUB) - 1, scratch, sizeof(scratch));
    TT_CHECK(e.has_pub);
    TT_CHECK(e.pub_known);
    TT_EQ_STR(e.fp, ED_FP);
    TT_EQ_INT(e.type, KeyTypeRsa);

    /* a public key dropped in as if it were the private one */
    keydir_inspect(
        &e, "id_ed25519", ED_PUB, sizeof(ED_PUB) - 1, NULL, 0, scratch, sizeof(scratch));
    TT_EQ_STR(e.problem, "This is a public key; copy the private key file too");
    char row[48];
    key_row_label(&e, false, row, sizeof(row));
    TT_EQ_STR(row, "id_ed25519 (not usable)");

    static const char junk[] = "hello";
    keydir_inspect(&e, "notes", junk, sizeof(junk) - 1, NULL, 0, scratch, sizeof(scratch));
    TT_EQ_STR(e.problem, "Not a private key file");
    for(size_t i = 0; i < sizeof(scratch); i++)
        TT_CHECK(scratch[i] == 0); /* nothing of a key file stays in the scratch buffer */
}

static void test_list_and_names(void) {
    KeyList l;
    keylist_init(&l);
    KeyEntry e = {0};
    const char* names[] = {"zeta", "Alpha", "id_ed25519", "beta"};
    for(int i = 0; i < 4; i++) {
        snprintf(e.name, sizeof(e.name), "%s", names[i]);
        TT_CHECK(keylist_add(&l, &e));
    }
    TT_EQ_STR(l.e[0].name, "Alpha");
    TT_EQ_STR(l.e[1].name, "beta");
    TT_EQ_STR(l.e[3].name, "zeta");
    TT_EQ_INT(keylist_find(&l, "beta"), 1);
    TT_EQ_INT(keylist_find(&l, "nope"), -1);

    char name[KEY_NAME_MAX + 1];
    TT_CHECK(key_name_next(&l, "id_ecdsa", '_', 2, name, sizeof(name)));
    TT_EQ_STR(name, "id_ecdsa");
    TT_CHECK(key_name_next(&l, "id_ed25519", '_', 2, name, sizeof(name)));
    TT_EQ_STR(name, "id_ed25519_2");
    snprintf(e.name, sizeof(e.name), "flipper");
    keylist_add(&l, &e);
    snprintf(e.name, sizeof(e.name), "flipper-1");
    keylist_add(&l, &e);
    TT_CHECK(key_name_next(&l, "flipper", '-', 1, name, sizeof(name)));
    TT_EQ_STR(name, "flipper-2");

    while(l.count < KEYS_MAX) {
        snprintf(e.name, sizeof(e.name), "k%u", l.count);
        keylist_add(&l, &e);
    }
    TT_CHECK(!keylist_add(&l, &e));

    KeyEntry d = {.name = "id_ed25519", .type = KeyTypeEd25519, .bits = 256};
    char row[48];
    key_row_label(&d, true, row, sizeof(row));
    TT_EQ_STR(row, "id_ed25519 (default)");
}

static void test_users(void) {
    char out[64];
    key_users_text(NULL, 0, out, sizeof(out));
    TT_EQ_STR(out, "No host uses it.");
    const char* one[] = {"nas"};
    key_users_text(one, 1, out, sizeof(out));
    TT_EQ_STR(out, "Used by: nas");
    const char* four[] = {"nas", "pi", "router", "vps"};
    key_users_text(four, 4, out, sizeof(out));
    TT_EQ_STR(out, "Used by: nas, pi (+2)");
    key_users_text(four, 2, out, sizeof(out));
    TT_EQ_STR(out, "Used by: nas, pi");
}

static HostProfile host(const char* text) {
    static char buf[1024];
    snprintf(
        buf,
        sizeof(buf),
        "Filetype: SSH Runner Host\nVersion: 1\nAlias: nas\nUser: u\nHost: 10.0.0.5\n%s",
        text);
    HostProfile p;
    TT_CHECK(profile_from_text(buf, &p));
    return p;
}

static void test_profile_auth(void) {
    /* every old and new value */
    TT_EQ_INT(host("Auth: password\n").auth, ProfileAuthPassword);
    TT_EQ_INT(host("").auth, ProfileAuthPassword);
    TT_EQ_INT(host("Auth: rubbish\n").auth, ProfileAuthPassword);
    TT_EQ_INT(
        host("Auth: key\n").auth, ProfileAuthDefault); /* v0: the ESP32's key = default now */
    TT_EQ_INT(host("Auth: default\n").auth, ProfileAuthDefault);
    HostProfile p = host("Auth: key\nKey: work\n");
    TT_EQ_INT(p.auth, ProfileAuthKey);
    TT_EQ_STR(p.key, "work");
    TT_EQ_INT(host("Auth: key\nKey: ../etc\n").auth, ProfileAuthDefault); /* invalid name */
    TT_EQ_INT(host("Auth: key\nKey: \n").auth, ProfileAuthDefault);
    TT_EQ_INT(host("Auth: password\nKey: work\n").auth, ProfileAuthPassword);

    /* writing */
    char text[1024];
    p = host("Auth: key\nKey: work\n");
    TT_CHECK(profile_to_text(&p, text, sizeof(text)));
    TT_CHECK(strstr(text, "Auth: key\nKey: work\n") != NULL);
    p.auth = ProfileAuthDefault;
    TT_CHECK(profile_to_text(&p, text, sizeof(text)));
    TT_CHECK(strstr(text, "Auth: default\n") != NULL);
    TT_CHECK(strstr(text, "Key:") == NULL);
    p.auth = ProfileAuthKey; /* without a name it is the default key */
    p.key[0] = '\0';
    TT_CHECK(profile_to_text(&p, text, sizeof(text)));
    TT_CHECK(strstr(text, "Auth: default\n") != NULL);

    /* wire auth and the key a host uses */
    p = host("Auth: password\n");
    TT_EQ_INT(profile_wire_auth(&p), ProtoAuthPassword);
    TT_CHECK(profile_key_name(&p, "id_ed25519") == NULL);
    p = host("Auth: default\n");
    TT_EQ_INT(profile_wire_auth(&p), ProtoAuthKey);
    TT_EQ_STR(profile_key_name(&p, "id_ed25519"), "id_ed25519");
    TT_CHECK(profile_key_name(&p, "") == NULL); /* no default set */
    TT_CHECK(key_used_by(&p, "id_ed25519", "id_ed25519"));
    TT_CHECK(!key_used_by(&p, "work", "id_ed25519"));
    p = host("Auth: key\nKey: work\n");
    TT_EQ_STR(profile_key_name(&p, "id_ed25519"), "work");
    TT_CHECK(key_used_by(&p, "work", "id_ed25519"));

    /* rename rewrites the host's Key:, delete leaves it (shown as missing) */
    TT_CHECK(profile_rename_key(&p, "work", "office"));
    TT_EQ_STR(p.key, "office");
    TT_CHECK(!profile_rename_key(&p, "work", "x"));
    TT_CHECK(!profile_rename_key(&p, "office", NULL));
    TT_EQ_STR(p.key, "office");
    HostProfile d = host("Auth: default\n");
    TT_CHECK(!profile_rename_key(&d, "id_ed25519", "x")); /* default hosts follow the setting */

    /* deleting a key in the app: its hosts switch to the default key, else password */
    p = host("Auth: key\nKey: work\n");
    TT_CHECK(profile_key_deleted(&p, "work", "id_ed25519"));
    TT_EQ_INT(p.auth, ProfileAuthDefault);
    p = host("Auth: key\nKey: work\n");
    TT_CHECK(profile_key_deleted(&p, "work", ""));
    TT_EQ_INT(p.auth, ProfileAuthPassword);
    p = host("Auth: key\nKey: other\n");
    TT_CHECK(!profile_key_deleted(&p, "work", "id_ed25519"));
    d = host("Auth: default\n"); /* the default itself deleted, no default left */
    TT_CHECK(profile_key_deleted(&d, "id_ed25519", ""));
    TT_EQ_INT(d.auth, ProfileAuthPassword);
    d = host("Auth: default\n"); /* another key deleted: unchanged */
    TT_CHECK(!profile_key_deleted(&d, "work", "id_ed25519"));
}

int main(void) {
    TT_RUN(test_names);
    TT_RUN(test_inspect);
    TT_RUN(test_list_and_names);
    TT_RUN(test_users);
    TT_RUN(test_profile_auth);
    TT_END();
}
