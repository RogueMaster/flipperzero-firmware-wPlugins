#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UHF_EPC_HEX_MAX     96U
#define UHF_TID_HEX_MAX     64U
#define UHF_USER_HEX_MAX    128U
#define UHF_KEY_SLOT_COUNT  4U
#define UHF_SETTINGS_MAGIC  0x55484632UL
#define UHF_KEY_VAULT_MAGIC 0x55484B31UL

typedef enum {
    UhfTagBankTid = 0,
    UhfTagBankEpc = 1,
    UhfTagBankUser = 2,
    UhfTagBankReserved = 3,
} UhfTagBank;

typedef enum {
    UhfStartupDefault = 0,
    UhfStartupRadar,
    UhfStartupInventory,
    UhfStartupTidDecoder,
    UhfStartupEpcFuzzing,
    UhfStartupCount,
} UhfStartupApp;

typedef enum {
    UhfEpcDisplayHex = 0,
    UhfEpcDisplayAscii,
    UhfEpcDisplayCount,
} UhfEpcDisplay;

typedef struct {
    uint32_t magic;
    uint8_t sound_enabled;
    uint8_t rf_power_dbm;
    uint8_t startup_app;
    uint8_t epc_display;
    uint8_t action_confirm;
    uint8_t reader_buzzer_enabled;
    uint8_t reader_buzzer_marker;
    uint8_t reserved;
} UhfSettingsData;

typedef struct {
    uint32_t magic;
    uint8_t active_slot;
    uint8_t reserved[3];
    uint8_t access_keys[UHF_KEY_SLOT_COUNT][4];
} UhfKeyVaultData;

typedef struct {
    bool used;
    char epc[UHF_EPC_HEX_MAX + 1];
    uint16_t read_count;
    uint16_t pc;
    uint32_t last_seen;
    uint32_t rssi;
    uint32_t signal_lower_since;
    uint32_t frequency;
    uint8_t antenna;
    uint8_t signal_bars;
} UhfTagEntry;
