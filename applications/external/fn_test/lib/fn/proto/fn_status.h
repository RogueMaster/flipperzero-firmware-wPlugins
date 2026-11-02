/**
 * @file fn_status.h
 * @brief Разбор ответов 30h (статус ФН) и 10h (смена и чек).
 */
#pragma once

#include "fn_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Снимок ответа 30h. */
typedef struct {
    FnStage stage;
    uint8_t stage_raw;
    FnDocCode current_doc;
    uint8_t current_doc_raw;
    bool doc_data_received;
    bool session_open;
    uint8_t warn_flags;
    FnDateTime last_doc_dt;
    char serial[17];
    uint32_t last_doc_number;
    bool has_last_doc;
    bool has_last_doc_number;
    bool has_serial;
} FnStatus;

/** Снимок ответа 10h. */
typedef struct {
    bool session_open;
    uint16_t shift_number;
    uint16_t receipt_number;
} FnSession;

/** 30h: len >= 30. При невалидной дате оба has_last_doc* = false. */
bool fn_parse_status_30h(const uint8_t* data, size_t len, FnStatus* out);

/** 10h: len == 5. */
bool fn_parse_session_10h(const uint8_t* data, size_t len, FnSession* out);

#ifdef __cplusplus
}
#endif
