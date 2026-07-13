#pragma once

#include "zk_vault.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    char default_name[ZK_NAME_LENGTH];
} ZkSettings;

void zk_settings_init(ZkSettings* settings);
bool zk_settings_load(ZkSettings* settings);
bool zk_settings_save(const ZkSettings* settings);

