#pragma once

#include "uhf_types.h"

/* Loading supplies defaults and migrates valid legacy records. */
void uhf_settings_load(UhfSettingsData* data);
bool uhf_settings_save(const UhfSettingsData* data);
void uhf_key_vault_load(UhfKeyVaultData* data);
bool uhf_key_vault_save(const UhfKeyVaultData* data);
