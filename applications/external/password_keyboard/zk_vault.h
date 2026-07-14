#pragma once

#include "zk_crypto.h"

#include <stdbool.h>
#include <stdint.h>

#define ZK_MAX_PASSWORDS       12
#define ZK_MAX_PASSWORD_LENGTH 64
#define ZK_NAME_LENGTH         24
#define ZK_DEFAULT_DAILY_LIMIT 3

enum {
    ZkClassLower = 1 << 0,
    ZkClassUpper = 1 << 1,
    ZkClassNumber = 1 << 2,
    ZkClassSpecial = 1 << 3,
};

enum {
    ZkPasswordHidden = 1 << 0,
};

typedef struct __attribute__((packed)) {
    char name[ZK_NAME_LENGTH];
    uint8_t flags;
    uint8_t length;
    uint8_t used_today;
    uint8_t daily_limit;
    uint32_t usage_day;
    uint8_t nonce[ZK_NONCE_SIZE];
    uint8_t cipher[ZK_MAX_PASSWORD_LENGTH];
    uint8_t tag[ZK_TAG_SIZE];
} ZkPasswordRecord;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t reserved;
    uint8_t legacy_daily_limit;
    uint8_t count;
    ZkPasswordRecord records[ZK_MAX_PASSWORDS];
} ZkVault;

_Static_assert(sizeof(ZkPasswordRecord) == 124, "Vault record layout changed");
_Static_assert(sizeof(ZkVault) == 1496, "Vault layout changed");

void zk_vault_init(ZkVault* vault);
bool zk_vault_load(ZkVault* vault);
bool zk_vault_save(const ZkVault* vault);
uint32_t zk_vault_today(void);
uint8_t zk_vault_remaining(ZkVault* vault, uint8_t index);
bool zk_vault_consume(ZkVault* vault, uint8_t index);
bool zk_vault_add(
    ZkVault* vault,
    const char* name,
    const char* password,
    bool hidden,
    uint8_t daily_limit,
    uint8_t already_used);
bool zk_vault_decrypt(const ZkPasswordRecord* record, char output[ZK_MAX_PASSWORD_LENGTH + 1]);
bool zk_vault_rename(ZkVault* vault, uint8_t index, const char* name);
bool zk_vault_delete(ZkVault* vault, uint8_t index);
uint8_t zk_vault_import(ZkVault* vault);

bool zk_password_generate(char* output, uint8_t length, uint8_t classes);
