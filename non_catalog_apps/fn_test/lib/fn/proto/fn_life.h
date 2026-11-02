/**
 * @file fn_life.h
 * @brief Разбор ответов жизненного цикла 32h, 3Bh, 3Dh и агрегат FnLifeInfo.
 */
#pragma once

#include "fn_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ответ 32h: срок действия и счетчики регистраций. */
typedef struct {
    FnDate end_date;
    uint8_t reg_remaining;
    uint8_t reg_done;
} FnLifeDate;

/** Ответ 3Bh: остаток дней; 0 — не значение. */
typedef struct {
    uint16_t days;
    bool has_days;
} FnLifeDays;

/** Ответ 3Dh: ресурс памяти (8 или 9 байт). */
typedef struct {
    uint32_t five_year_documents;
    uint32_t thirty_day_kb; /**< ресурс 30-дневного хранения, КБ */
    uint8_t marking_percent;
    bool has_marking;
} FnMemory;

/** 32h: len == 5. */
bool fn_parse_end_date_32h(const uint8_t* data, size_t len, FnLifeDate* out);

/** 3Bh: len == 2; days == 0 → has_days == false. */
bool fn_parse_end_days_3bh(const uint8_t* data, size_t len, FnLifeDays* out);

/** 3Dh: len == 8 или 9; байт 8 читается только при len >= 9. */
bool fn_parse_memory_3dh(const uint8_t* data, size_t len, FnMemory* out);

/** Агрегат экрана «Информация о фазе жизни ФН». */
typedef struct {
    FnLifeDate date;
    FnLifeDays days;
    FnMemory memory;
    bool has_date;
    bool has_days;
    bool has_memory;
} FnLifeInfo;

FnLifeInfo* fn_life_info_alloc(void);
void fn_life_info_free(FnLifeInfo* life);
void fn_life_info_reset(FnLifeInfo* life);

#ifdef __cplusplus
}
#endif
