#include "uhf_settings.h"
#include "uhf_storage.h"

#include <storage/storage.h>
#include <stddef.h>

#define UHF_DATA_DIR STORAGE_APP_DATA_PATH_PREFIX
#define UHF_LEGACY_DIR APP_DATA_PATH("uhf_expansion")

bool uhf_settings_save(const UhfSettingsData* data) {
    return data && uhf_storage_write_blob(
                       UHF_DATA_DIR, UHF_DATA_DIR "/settings.bin", data, sizeof(*data));
}

static bool uhf_settings_read(const char* path, UhfSettingsData* record) {
    if(uhf_storage_read_blob(path, record, sizeof(*record)) && record->magic == UHF_SETTINGS_MAGIC)
        return true;
    /* Original settings were eight bytes; missing Action Confirm defaults to Yes. */
    *record = (UhfSettingsData){.action_confirm = 1U};
    return uhf_storage_read_blob(path, record, offsetof(UhfSettingsData, action_confirm)) &&
           record->magic == UHF_SETTINGS_MAGIC;
}

void uhf_settings_load(UhfSettingsData* data) {
    if(!data) return;
    UhfSettingsData record = {0};
    bool valid = uhf_settings_read(UHF_DATA_DIR "/settings.bin", &record);
    if(!valid) {
        valid = uhf_settings_read(UHF_LEGACY_DIR "/settings.bin", &record);
        if(valid) (void)uhf_settings_save(&record);
    }
    *data = (UhfSettingsData){
        .magic = UHF_SETTINGS_MAGIC,
        .sound_enabled = 1U,
        .rf_power_dbm = 20U,
        .startup_app = UhfStartupDefault,
        .epc_display = UhfEpcDisplayHex,
        .action_confirm = 1U,
    };
    if(!valid) return;
    data->sound_enabled = record.sound_enabled != 0U;
    if(record.action_confirm <= 1U) data->action_confirm = record.action_confirm;
    if(record.rf_power_dbm <= 20U) data->rf_power_dbm = record.rf_power_dbm;
    if(record.startup_app < UhfStartupCount) data->startup_app = record.startup_app;
    if(record.epc_display < UhfEpcDisplayCount) data->epc_display = record.epc_display;
}

bool uhf_key_vault_save(const UhfKeyVaultData* data) {
    return data && uhf_storage_write_blob(
                       UHF_DATA_DIR, UHF_DATA_DIR "/keys.bin", data, sizeof(*data));
}

void uhf_key_vault_load(UhfKeyVaultData* data) {
    if(!data) return;
    UhfKeyVaultData record = {0};
    bool valid = uhf_storage_read_blob(UHF_DATA_DIR "/keys.bin", &record, sizeof(record)) &&
                 record.magic == UHF_KEY_VAULT_MAGIC;
    if(!valid) {
        valid = uhf_storage_read_blob(UHF_LEGACY_DIR "/keys.bin", &record, sizeof(record)) &&
                record.magic == UHF_KEY_VAULT_MAGIC;
        if(valid) (void)uhf_key_vault_save(&record);
    }
    *data = valid ? record : (UhfKeyVaultData){.magic = UHF_KEY_VAULT_MAGIC};
    if(data->active_slot >= UHF_KEY_SLOT_COUNT) data->active_slot = 0U;
}
