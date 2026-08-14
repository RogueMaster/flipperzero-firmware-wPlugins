#include "ck42x_vault_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void append_named(CkVaultStore* store, size_t index) {
    CkVaultEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.account, sizeof(entry.account), "acct%zu", index);
    snprintf(entry.username, sizeof(entry.username), "user%zu", index);
    snprintf(entry.password, sizeof(entry.password), "pass-value-%zu", index);
    if(!ck_vault_store_append(store, &entry)) fail("append failed");
}

int main(void) {
    CkVaultStore store;
    ck_vault_store_init(&store);

    if(store.count != 0 || store.entries != NULL) fail("init should be empty");

    for(size_t i = 0; i < 50; i++)
        append_named(&store, i);
    if(store.count != 50) fail("expected 50 appended entries");
    if(store.capacity < 50) fail("capacity should grow past the old 20-entry cap");
    if(strcmp(store.entries[0].account, "acct0") != 0) fail("first account");
    if(strcmp(store.entries[20].password, "pass-value-20") != 0) fail("entry 20");
    if(strcmp(store.entries[49].username, "user49") != 0) fail("last username");

    uint8_t* serialized = NULL;
    size_t serialized_len = 0;
    if(!ck_vault_store_serialize(&store, &serialized, &serialized_len)) fail("serialize 50");
    if(!serialized || serialized_len == 0) fail("serialized payload");

    CkVaultStore loaded;
    ck_vault_store_init(&loaded);
    char* buf = malloc(serialized_len + 1);
    if(!buf) fail("malloc parse buffer");
    memcpy(buf, serialized, serialized_len);
    buf[serialized_len] = '\0';
    if(!ck_vault_store_parse(&loaded, buf)) fail("parse 50");
    if(loaded.count != 50) fail("parsed count");
    if(strcmp(loaded.entries[21].account, "acct21") != 0) fail("round-trip account 21");
    if(strcmp(loaded.entries[49].password, "pass-value-49") != 0) fail("round-trip password 49");

    ck_vault_store_reset(&loaded);
    if(loaded.count != 0) fail("reset should clear count");
    if(loaded.entries == NULL) fail("reset keeps allocation");

    char mixed[] = "skip-me\n"
                   "ok\tuser\tsecret\n"
                   "also-skip\n"
                   "two\tname\tvalue\n";
    if(!ck_vault_store_parse(&loaded, mixed)) fail("parse mixed");
    if(loaded.count != 2) fail("mixed valid count");
    if(strcmp(loaded.entries[0].account, "ok") != 0) fail("mixed first");
    if(strcmp(loaded.entries[1].password, "value") != 0) fail("mixed second");

    ck_vault_store_reset(&store);
    uint8_t* empty = NULL;
    size_t empty_len = 0;
    if(!ck_vault_store_serialize(&store, &empty, &empty_len)) fail("serialize empty");
    if(empty_len != 0) fail("empty payload length");

    for(size_t i = 0; i < 128; i++)
        append_named(&store, i);
    if(store.count != 128) fail("expected 128 entries");

    free(serialized);
    free(buf);
    free(empty);
    ck_vault_store_free(&store);
    ck_vault_store_free(&loaded);
    printf("OK: vault store holds more than 20 passwords\n");
    return 0;
}
