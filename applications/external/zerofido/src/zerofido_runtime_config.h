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

#pragma once

#include <stdbool.h>
#include <storage/storage.h>
#include <stdint.h>

#include "zerofido_types.h"

typedef struct ZerofidoApp ZerofidoApp;

typedef enum {
    ZfTransportModeUsbHid = 0,
    ZfTransportModeNfc = 1,
} ZfTransportMode;

typedef enum {
    ZfFido2ProfileCtap2_0 = 0,
    ZfFido2ProfileCtap2_1Experimental = 1,
    ZfFido2ProfileCurrent = ZfFido2ProfileCtap2_0,
} ZfFido2Profile;

typedef enum {
    ZfU2fProfileCurrent = 0,
} ZfU2fProfile;

typedef struct {
    ZfTransportMode transport_mode;
    bool fido2_enabled;
    ZfFido2Profile fido2_profile;
    bool u2f_enabled;
    ZfU2fProfile u2f_profile;
    bool auto_accept_requests;
    ZfAttestationMode attestation_mode;
} ZfRuntimeConfig;

typedef struct {
    bool usb_hid_enabled;
    bool nfc_enabled;
    bool fido2_enabled;
    bool u2f_enabled;
    bool client_pin_enabled;
    bool selection_enabled;
    bool transport_keepalive_enabled;
    bool transport_cancel_enabled;
    bool transport_wink_enabled;
    ZfFido2Profile fido2_profile;
    bool advertise_fido_2_1;
    bool advertise_fido_2_0;
    bool advertise_u2f_v2;
    bool pin_uv_auth_token_enabled;
    bool pin_uv_auth_protocol_2_enabled;
    bool client_pin_token_requires_consent;
    bool make_cred_uv_not_required;
    bool advertise_usb_transport;
    bool advertise_nfc_transport;
    bool auto_accept_requests;
    ZfAttestationMode attestation_mode;
} ZfResolvedCapabilities;

/*
 * Runtime config is the persisted user preference layer. Capability resolution
 * combines those preferences with compile-time transport/profile flags so
 * protocol dispatch and getInfo advertise the same effective behavior.
 */
void zf_runtime_config_load_defaults(ZfRuntimeConfig *config);
void zf_runtime_config_load(Storage *storage, ZfRuntimeConfig *config);
bool zf_runtime_config_persist(Storage *storage, const ZfRuntimeConfig *config);
void zf_runtime_config_apply(ZerofidoApp *app, const ZfRuntimeConfig *config);
void zf_runtime_config_refresh_capabilities(ZerofidoApp *app);
bool zf_runtime_config_set_auto_accept_requests(ZerofidoApp *app, Storage *storage, bool enabled);
bool zf_runtime_config_set_fido2_enabled(ZerofidoApp *app, Storage *storage, bool enabled);
bool zf_runtime_config_set_fido2_profile(ZerofidoApp *app, Storage *storage,
                                         ZfFido2Profile profile);
bool zf_runtime_config_set_attestation_mode(ZerofidoApp *app, Storage *storage,
                                            ZfAttestationMode mode);
bool zf_runtime_config_set_transport_mode(ZerofidoApp *app, Storage *storage, ZfTransportMode mode);
void zf_runtime_config_resolve_capabilities(const ZfRuntimeConfig *config,
                                            ZfResolvedCapabilities *capabilities);
void zf_runtime_get_effective_capabilities(const ZerofidoApp *app,
                                           ZfResolvedCapabilities *capabilities);
bool zf_runtime_ctap_command_enabled(const ZerofidoApp *app, uint8_t cmd);
const char *zf_transport_mode_name(ZfTransportMode mode);
const char *zf_fido2_profile_name(ZfFido2Profile profile);
const char *zf_attestation_mode_name(ZfAttestationMode mode);
