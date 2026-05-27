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

#include "../zerofido_types.h"

/*
 * Recovery helpers clean interrupted writes and remove every companion path for
 * a credential record: primary, temp, backup, counter floor, and temp floor.
 */
void zf_store_recovery_cleanup_temp_files(Storage *storage);
void zf_store_recovery_cleanup_temp_files_with_buffer(Storage *storage, uint8_t *buffer,
                                                      size_t buffer_size);
bool zf_store_recovery_remove_record_paths(Storage *storage, const char *file_name);
