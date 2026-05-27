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

#include <stddef.h>
#include <stdint.h>

#include "../zerofido_types.h"

/*
 * CTAP request parsers validate CBOR shape, required fields, duplicate keys,
 * and bounded copies into fixed-size request structs. Semantic policy checks
 * happen later in command/policy modules.
 */
uint8_t zf_ctap_parse_make_credential(const uint8_t *data, size_t size,
                                      ZfMakeCredentialRequest *request);
uint8_t zf_ctap_parse_get_assertion(const uint8_t *data, size_t size,
                                    ZfGetAssertionRequest *request);
bool zf_ctap_descriptor_list_contains_id(const ZfCredentialDescriptorList *list,
                                         const uint8_t *credential_id, size_t credential_id_len);
