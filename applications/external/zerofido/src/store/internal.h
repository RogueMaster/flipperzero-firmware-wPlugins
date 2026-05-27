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
#include <stdio.h>
#include <string.h>

#include "../zerofido_types.h"

#define ZF_STORE_RECORD_MAX_SIZE 800

/*
 * Shared record-path builders keep primary, temp, backup, and counter-floor
 * files in one app-data directory with predictable suffixes.
 */
static inline bool zf_store_has_suffix(const char *value, const char *suffix) {
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);

    return value_len >= suffix_len && strcmp(value + value_len - suffix_len, suffix) == 0;
}

static inline void zf_store_build_record_path(const char *file_name, char *path, size_t path_size) {
    snprintf(path, path_size, ZF_APP_DATA_DIR "/%s", file_name);
}

static inline void zf_store_build_temp_path(const char *file_name, char *path, size_t path_size) {
    snprintf(path, path_size, ZF_APP_DATA_DIR "/%s.tmp", file_name);
}

static inline void zf_store_build_backup_path(const char *file_name, char *path, size_t path_size) {
    snprintf(path, path_size, ZF_APP_DATA_DIR "/%s.bak", file_name);
}

static inline void zf_store_build_counter_floor_path(const char *file_name, char *path,
                                                     size_t path_size) {
    snprintf(path, path_size, ZF_APP_DATA_DIR "/%s.counter", file_name);
}

static inline void zf_store_build_counter_floor_temp_path(const char *file_name, char *path,
                                                          size_t path_size) {
    snprintf(path, path_size, ZF_APP_DATA_DIR "/%s.counter.tmp", file_name);
}
