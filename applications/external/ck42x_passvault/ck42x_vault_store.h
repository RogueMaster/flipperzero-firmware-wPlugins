#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CK_ACCOUNT_LEN  32
#define CK_USERNAME_LEN 48
#define CK_PASSWORD_LEN 72
#define CK_LINE_MAX     180

typedef struct {
    char account[CK_ACCOUNT_LEN];
    char username[CK_USERNAME_LEN];
    char password[CK_PASSWORD_LEN];
} CkVaultEntry;

typedef struct {
    CkVaultEntry* entries;
    size_t count;
    size_t capacity;
} CkVaultStore;

void ck_vault_store_init(CkVaultStore* store);
void ck_vault_store_reset(CkVaultStore* store);
void ck_vault_store_free(CkVaultStore* store);
bool ck_vault_store_reserve(CkVaultStore* store, size_t needed);
bool ck_vault_store_append(CkVaultStore* store, const CkVaultEntry* entry);
bool ck_vault_store_parse(CkVaultStore* store, char* buf);
bool ck_vault_store_serialize(const CkVaultStore* store, uint8_t** out, size_t* out_len);
