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
#include <stddef.h>
#include <stdint.h>

#include "../zerofido_types.h"
#include <storage/storage.h>

typedef struct ZerofidoApp ZerofidoApp;

/*
 * CTAP policy helpers answer questions that depend on request options, PIN/UV
 * state, credential protection, and store visibility. Keeping them separate
 * keeps parsers mechanical and response builders side-effect free.
 */
bool zf_ctap_request_uses_allow_list(const ZfGetAssertionRequest *request);
uint8_t zf_ctap_validate_pin_auth_protocol(bool has_pin_auth, bool has_pin_protocol,
                                           uint64_t pin_protocol, bool allow_protocol2);
bool zf_ctap_effective_uv_requested(bool has_pin_auth, bool has_uv, bool uv);
uint8_t zf_ctap_require_empty_payload(size_t request_len);
bool zf_ctap_local_maintenance_busy(ZerofidoApp *app);
bool zf_ctap_pin_is_set(ZerofidoApp *app);
bool zf_ctap_store_entry_matches_descriptor_list(const ZfCredentialIndexEntry *entry,
                                                 const void *context);
bool zf_ctap_exclude_list_has_visible_match(Storage *storage, const ZfCredentialStore *store,
                                            const char *rp_id,
                                            const ZfCredentialDescriptorList *exclude_list,
                                            bool uv_verified, uint8_t *buffer, size_t buffer_size);
size_t zf_ctap_resolve_assertion_matches(Storage *storage, ZfCredentialStore *store,
                                         const ZfGetAssertionRequest *request, bool uv_verified,
                                         uint16_t *match_indices);
