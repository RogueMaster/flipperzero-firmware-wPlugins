/**
 * @file fn_ident.h
 * @brief Разбор ответов 3Ah (версия ФФД) и 33h (версия ПО ФН).
 */
#pragma once

#include "fn_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ответ 3Ah: текущая и максимальная версия ФФД. */
typedef struct {
    FnFfd current;
    uint8_t current_raw;
    FnFfd max;
    uint8_t max_raw;
} FnFfdInfo;

/** Ответ 33h: версия ПО и признак опытного образца. */
typedef struct {
    char version[17];
    uint8_t mode_raw; /**< 0 — опытный образец, 1 — серийное изделие */
    bool is_mgm; /**< эвристика проекта: mode_raw == 0 */
} FnFwInfo;

/** 3Ah: len == 2. */
bool fn_parse_ffd_3ah(const uint8_t* data, size_t len, FnFfdInfo* out);

/** 33h: len == 17 (16 байт версии + признак режима). */
bool fn_parse_fw_33h(const uint8_t* data, size_t len, FnFwInfo* out);

#ifdef __cplusplus
}
#endif
