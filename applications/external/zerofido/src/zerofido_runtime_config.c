/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "zerofido_runtime_config.h"

#include <string.h>

#include "zerofido_storage.h"

#include "zerofido_app_i.h"
#include "zerofido_types.h"

#define ZF_RUNTIME_CONFIG_FILE_PATH ZF_APP_DATA_DIR "/runtime_config.bin"
#define ZF_RUNTIME_CONFIG_FILE_TEMP_PATH ZF_APP_DATA_DIR "/runtime_config.tmp"
#define ZF_RUNTIME_CONFIG_FILE_MAGIC 0x5A465243UL
#define ZF_RUNTIME_CONFIG_FILE_VERSION 4U
#define ZF_RUNTIME_CONFIG_FILE_VERSION_3_SIZE 8U
#define ZF_RUNTIME_CONFIG_FLAG_AUTO_ACCEPT_REQUESTS 0x01U
#define ZF_RUNTIME_CONFIG_FLAG_FIDO2_ENABLED 0x02U

/*
 * Versioned binary runtime config. Unknown flags or invalid enum values reject
 * the file and fall back to defaults; v1/v2 records are migrated in memory by
 * filling fields that did not exist in those versions.
 */
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint8_t transport_mode;
    uint8_t fido2_profile;
    uint8_t attestation_mode;
} ZfRuntimeConfigFileRecord;

static bool zf_transport_mode_is_valid(uint8_t mode) {
#ifdef ZF_USB_ONLY
    return mode == ZfTransportModeUsbHid;
#elif defined(ZF_NFC_ONLY)
    return mode == ZfTransportModeNfc;
#else
    return mode == ZfTransportModeUsbHid || mode == ZfTransportModeNfc;
#endif
}

static ZfTransportMode zf_runtime_config_default_transport_mode(void) {
#ifdef ZF_NFC_ONLY
    return ZfTransportModeNfc;
#else
    return ZfTransportModeUsbHid;
#endif
}

static bool zf_fido2_profile_is_valid(uint8_t profile) {
    return profile == ZfFido2ProfileCtap2_0 || profile == ZfFido2ProfileCtap2_1Experimental;
}

static ZfFido2Profile zf_fido2_profile_for_build(ZfFido2Profile profile) {
#if ZF_DEV_FIDO2_1
    return profile;
#else
    (void)profile;
    return ZfFido2ProfileCtap2_0;
#endif
}

static bool zf_attestation_mode_is_valid(uint8_t mode) {
    return mode == ZfAttestationModePacked || mode == ZfAttestationModeNone;
}

static ZfAttestationMode zf_attestation_mode_for_build(ZfAttestationMode mode) {
#if ZF_PACKED_ATTESTATION
    return mode;
#else
    (void)mode;
    return ZfAttestationModeNone;
#endif
}

static bool zf_attestation_mode_is_settable(ZfAttestationMode mode) {
    if (!zf_attestation_mode_is_valid((uint8_t)mode)) {
        return false;
    }
#if ZF_PACKED_ATTESTATION
    return true;
#else
    return mode == ZfAttestationModeNone;
#endif
}

void zf_runtime_config_load_defaults(ZfRuntimeConfig *config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->transport_mode = zf_runtime_config_default_transport_mode();
    config->fido2_enabled = true;
    config->fido2_profile = ZfFido2ProfileCurrent;
    config->u2f_enabled = true;
    config->u2f_profile = ZfU2fProfileCurrent;
    config->auto_accept_requests = false;
    config->attestation_mode = ZfAttestationModeNone;
}

void zf_runtime_config_load(Storage *storage, ZfRuntimeConfig *config) {
    ZfRuntimeConfigFileRecord record = {0};
    size_t size = 0;

    zf_runtime_config_load_defaults(config);
    if (!storage || !config) {
        return;
    }
    zf_storage_recover_atomic_file(storage, ZF_RUNTIME_CONFIG_FILE_PATH,
                                   ZF_RUNTIME_CONFIG_FILE_TEMP_PATH);

    if (!zf_storage_read_file(storage, ZF_RUNTIME_CONFIG_FILE_PATH, (uint8_t *)&record,
                              sizeof(record), &size)) {
        return;
    }
    if (size != ZF_RUNTIME_CONFIG_FILE_VERSION_3_SIZE && size != sizeof(record)) {
        return;
    }

    if (record.magic != ZF_RUNTIME_CONFIG_FILE_MAGIC) {
        return;
    }

    if ((record.flags & ~(ZF_RUNTIME_CONFIG_FLAG_AUTO_ACCEPT_REQUESTS |
                          ZF_RUNTIME_CONFIG_FLAG_FIDO2_ENABLED)) != 0) {
        return;
    }

    if (record.version == 1U) {
        config->transport_mode = zf_runtime_config_default_transport_mode();
        config->fido2_profile = ZfFido2ProfileCtap2_0;
        config->attestation_mode = ZfAttestationModeNone;
    } else if (record.version == 2U) {
        if (!zf_transport_mode_is_valid(record.transport_mode)) {
            return;
        }
        config->transport_mode = (ZfTransportMode)record.transport_mode;
        if (record.fido2_profile != 0U) {
            return;
        }
        config->fido2_profile = ZfFido2ProfileCtap2_0;
        config->attestation_mode = ZfAttestationModeNone;
    } else if (record.version == 3U || record.version == ZF_RUNTIME_CONFIG_FILE_VERSION) {
        if (!zf_transport_mode_is_valid(record.transport_mode) ||
            !zf_fido2_profile_is_valid(record.fido2_profile)) {
            return;
        }
        config->transport_mode = (ZfTransportMode)record.transport_mode;
        config->fido2_profile = (ZfFido2Profile)record.fido2_profile;
        if (record.version == ZF_RUNTIME_CONFIG_FILE_VERSION) {
            if (size != sizeof(record) || !zf_attestation_mode_is_valid(record.attestation_mode)) {
                return;
            }
            config->attestation_mode = (ZfAttestationMode)record.attestation_mode;
        } else {
            config->attestation_mode = ZfAttestationModeNone;
        }
    } else {
        return;
    }

    config->fido2_profile = zf_fido2_profile_for_build(config->fido2_profile);
    config->attestation_mode = zf_attestation_mode_for_build(config->attestation_mode);
#if ZF_AUTO_ACCEPT_REQUESTS
    config->auto_accept_requests =
        (record.flags & ZF_RUNTIME_CONFIG_FLAG_AUTO_ACCEPT_REQUESTS) != 0;
#else
    config->auto_accept_requests = false;
#endif
    config->fido2_enabled = true;
}

bool zf_runtime_config_persist(Storage *storage, const ZfRuntimeConfig *config) {
    ZfRuntimeConfigFileRecord record = {
        .magic = ZF_RUNTIME_CONFIG_FILE_MAGIC,
        .version = ZF_RUNTIME_CONFIG_FILE_VERSION,
        .flags = config
                     ? (((ZF_AUTO_ACCEPT_REQUESTS && config->auto_accept_requests)
                             ? ZF_RUNTIME_CONFIG_FLAG_AUTO_ACCEPT_REQUESTS
                             : 0U) |
                        ZF_RUNTIME_CONFIG_FLAG_FIDO2_ENABLED)
                     : 0U,
        .transport_mode = config ? (uint8_t)config->transport_mode
                                 : (uint8_t)zf_runtime_config_default_transport_mode(),
        .fido2_profile = config ? (uint8_t)zf_fido2_profile_for_build(config->fido2_profile) :
                                  (uint8_t)ZfFido2ProfileCtap2_0,
        .attestation_mode =
            config ? (uint8_t)zf_attestation_mode_for_build(config->attestation_mode) :
                     (uint8_t)ZfAttestationModeNone,
    };

    if (!storage || !config || !zf_fido2_profile_is_valid((uint8_t)config->fido2_profile) ||
        !zf_attestation_mode_is_valid((uint8_t)config->attestation_mode) ||
        !zf_storage_ensure_app_data_dir(storage)) {
        return false;
    }

    return zf_storage_write_file_atomic(storage, ZF_RUNTIME_CONFIG_FILE_PATH,
                                        ZF_RUNTIME_CONFIG_FILE_TEMP_PATH, (const uint8_t *)&record,
                                        sizeof(record));
}

static void zf_runtime_config_resolve_app_capabilities(ZerofidoApp *app,
                                                       const ZfRuntimeConfig *config) {
    ZfRuntimeConfig effective_config;

    if (!app || !config) {
        return;
    }

    effective_config = *config;
#if !ZF_AUTO_ACCEPT_REQUESTS
    effective_config.auto_accept_requests = false;
#endif
    effective_config.fido2_profile = zf_fido2_profile_for_build(effective_config.fido2_profile);
    if (effective_config.fido2_profile == ZfFido2ProfileCtap2_1Experimental &&
        !zerofido_pin_is_set(&app->pin_state)) {
        effective_config.fido2_profile = ZfFido2ProfileCtap2_0;
    }

    zf_runtime_config_resolve_capabilities(&effective_config, &app->capabilities);
    app->capabilities_resolved = true;
}

/*
 * Applies the persisted/user-requested config, then resolves runtime
 * capabilities against compile-time feature flags and the current PIN state.
 * CTAP 2.1 experimental is honored only in developer builds and only while a
 * PIN exists.
 */
void zf_runtime_config_apply(ZerofidoApp *app, const ZfRuntimeConfig *config) {
    if (!app || !config) {
        return;
    }

    app->runtime_config = *config;
#if !ZF_AUTO_ACCEPT_REQUESTS
    app->runtime_config.auto_accept_requests = false;
#endif
    app->runtime_config.fido2_profile =
        zf_fido2_profile_for_build(app->runtime_config.fido2_profile);
    app->runtime_config.attestation_mode =
        zf_attestation_mode_for_build(app->runtime_config.attestation_mode);
    zf_runtime_config_resolve_app_capabilities(app, &app->runtime_config);
}

void zf_runtime_config_refresh_capabilities(ZerofidoApp *app) {
    if (!app) {
        return;
    }

    zf_runtime_config_resolve_app_capabilities(app, &app->runtime_config);
}

bool zf_runtime_config_set_auto_accept_requests(ZerofidoApp *app, Storage *storage, bool enabled) {
    ZfRuntimeConfig next_config;

    if (!app) {
        return false;
    }
#if !ZF_AUTO_ACCEPT_REQUESTS
    (void)storage;
    (void)enabled;
    return false;
#endif

    next_config = app->runtime_config;
    next_config.auto_accept_requests = enabled;
    if (!zf_runtime_config_persist(storage, &next_config)) {
        return false;
    }

    zf_runtime_config_apply(app, &next_config);
    return true;
}

bool zf_runtime_config_set_fido2_enabled(ZerofidoApp *app, Storage *storage, bool enabled) {
    ZfRuntimeConfig next_config;

    if (!app) {
        return false;
    }
    (void)enabled;

    next_config = app->runtime_config;
    next_config.fido2_enabled = true;
    if (!zf_runtime_config_persist(storage, &next_config)) {
        return false;
    }

    zf_runtime_config_apply(app, &next_config);
    return true;
}

bool zf_runtime_config_set_fido2_profile(ZerofidoApp *app, Storage *storage,
                                         ZfFido2Profile profile) {
    ZfRuntimeConfig next_config;

    if (!app || !zf_fido2_profile_is_valid((uint8_t)profile)) {
        return false;
    }
#if !ZF_DEV_FIDO2_1
    if (profile == ZfFido2ProfileCtap2_1Experimental) {
        return false;
    }
#endif
    if (profile == ZfFido2ProfileCtap2_1Experimental && !zerofido_pin_is_set(&app->pin_state)) {
        return false;
    }

    next_config = app->runtime_config;
    next_config.fido2_profile = profile;
    if (!zf_runtime_config_persist(storage, &next_config)) {
        return false;
    }

    zf_runtime_config_apply(app, &next_config);
    return true;
}

bool zf_runtime_config_set_attestation_mode(ZerofidoApp *app, Storage *storage,
                                            ZfAttestationMode mode) {
    ZfRuntimeConfig next_config;

    if (!app || !zf_attestation_mode_is_settable(mode)) {
        return false;
    }

    next_config = app->runtime_config;
    next_config.attestation_mode = mode;
    if (!zf_runtime_config_persist(storage, &next_config)) {
        return false;
    }

    zf_runtime_config_apply(app, &next_config);
    return true;
}

bool zf_runtime_config_set_transport_mode(ZerofidoApp *app, Storage *storage,
                                          ZfTransportMode mode) {
    ZfRuntimeConfig next_config;

    if (!app || !zf_transport_mode_is_valid((uint8_t)mode)) {
        return false;
    }

    next_config = app->runtime_config;
    next_config.transport_mode = mode;
    if (!zf_runtime_config_persist(storage, &next_config)) {
        return false;
    }

    zf_runtime_config_apply(app, &next_config);
    return true;
}

/*
 * Central profile-to-capability map. Compile-time transport macros constrain
 * the effective transport surface even when persisted config requests another
 * mode.
 */
void zf_runtime_config_resolve_capabilities(const ZfRuntimeConfig *config,
                                            ZfResolvedCapabilities *capabilities) {
    bool usb_hid_enabled = false;
    bool nfc_enabled = false;

    if (!config || !capabilities) {
        return;
    }

#ifdef ZF_USB_ONLY
    usb_hid_enabled = true;
    nfc_enabled = false;
#elif defined(ZF_NFC_ONLY)
    usb_hid_enabled = false;
    nfc_enabled = true;
#else
    usb_hid_enabled = config->transport_mode == ZfTransportModeUsbHid;
    nfc_enabled = config->transport_mode == ZfTransportModeNfc;
#endif

    memset(capabilities, 0, sizeof(*capabilities));
    capabilities->usb_hid_enabled = usb_hid_enabled;
    capabilities->nfc_enabled = nfc_enabled;
    capabilities->fido2_enabled = true;
    capabilities->u2f_enabled = config->u2f_enabled;
    capabilities->client_pin_enabled = true;
    capabilities->transport_keepalive_enabled = usb_hid_enabled;
    capabilities->transport_cancel_enabled = usb_hid_enabled;
    capabilities->transport_wink_enabled = usb_hid_enabled && config->u2f_enabled;
    capabilities->fido2_profile = zf_fido2_profile_for_build(config->fido2_profile);
    capabilities->advertise_fido_2_1 =
        capabilities->fido2_profile == ZfFido2ProfileCtap2_1Experimental;
#if !ZF_DEV_FIDO2_1
    capabilities->advertise_fido_2_1 = false;
#endif
    capabilities->advertise_fido_2_0 = true;
    capabilities->advertise_u2f_v2 = config->u2f_enabled;
    capabilities->pin_uv_auth_token_enabled = capabilities->advertise_fido_2_1;
    capabilities->pin_uv_auth_protocol_2_enabled = capabilities->advertise_fido_2_1;
    capabilities->selection_enabled =
        capabilities->fido2_enabled && capabilities->advertise_fido_2_1;
    capabilities->client_pin_token_requires_consent = capabilities->advertise_fido_2_1;
    capabilities->make_cred_uv_not_required = capabilities->advertise_fido_2_1;
    capabilities->advertise_usb_transport = usb_hid_enabled;
    capabilities->advertise_nfc_transport = nfc_enabled;
    capabilities->auto_accept_requests = ZF_AUTO_ACCEPT_REQUESTS && config->auto_accept_requests;
    capabilities->attestation_mode = zf_attestation_mode_for_build(config->attestation_mode);
}

void zf_runtime_get_effective_capabilities(const ZerofidoApp *app,
                                           ZfResolvedCapabilities *capabilities) {
    ZfRuntimeConfig defaults;

    if (!capabilities) {
        return;
    }

    if (app && app->capabilities_resolved) {
        *capabilities = app->capabilities;
        return;
    }

    zf_runtime_config_load_defaults(&defaults);
    zf_runtime_config_resolve_capabilities(&defaults, capabilities);
}

bool zf_runtime_ctap_command_enabled(const ZerofidoApp *app, uint8_t cmd) {
    ZfResolvedCapabilities capabilities;

    zf_runtime_get_effective_capabilities(app, &capabilities);
    switch (cmd) {
    case ZfCtapeCmdGetInfo:
    case ZfCtapeCmdMakeCredential:
    case ZfCtapeCmdGetAssertion:
    case ZfCtapeCmdReset:
    case ZfCtapeCmdGetNextAssertion:
        return capabilities.fido2_enabled;
    case ZfCtapeCmdClientPin:
        return capabilities.client_pin_enabled;
    case ZfCtapeCmdSelection:
        return capabilities.selection_enabled;
    default:
        return false;
    }
}

const char *zf_transport_mode_name(ZfTransportMode mode) {
    switch (mode) {
    case ZfTransportModeNfc:
        return "NFC";
    case ZfTransportModeUsbHid:
    default:
        return "USB HID";
    }
}

const char *zf_fido2_profile_name(ZfFido2Profile profile) {
    switch (profile) {
    case ZfFido2ProfileCtap2_1Experimental:
        return "2.1 exp";
    case ZfFido2ProfileCtap2_0:
    default:
        return "2.0";
    }
}

const char *zf_attestation_mode_name(ZfAttestationMode mode) {
    switch (mode) {
    case ZfAttestationModeNone:
        return "None";
#if ZF_PACKED_ATTESTATION
    case ZfAttestationModePacked:
    default:
        return "Packed";
#else
    case ZfAttestationModePacked:
    default:
        return "None";
#endif
    }
}
