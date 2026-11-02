/**
 * @file fn_types.h
 * @brief Базовые типы протокола ФН: дата/время, этапы, ФФД, документы, флаги.
 *
 * Модуль чистого ядра: не включает furi/gui, пригоден для host-тестов
 * (contracts/module-boundaries.md §1).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Дата: две последние цифры года, месяц и день. */
typedef struct {
    uint8_t year; /**< 0–99 */
    uint8_t month; /**< 1–12 (исправлено с ошибочного `mouth`) */
    uint8_t day; /**< 1–31 */
} FnDate;

/** Дата и время документа. */
typedef struct {
    FnDate date;
    uint8_t hour; /**< 0–23 */
    uint8_t minute; /**< 0–59 */
} FnDateTime;

/** Этап жизни ФН (поле «Состояние ФН» ответа 30h). */
typedef enum {
    FnStageUnknown = 0x00, /**< значение не описано протоколом */
    FnStageReadyRegistration = 0x01, /**< готов к регистрации */
    FnStageRegistered = 0x03, /**< зарегистрирован (этап 2) */
    FnStageArchiveClosed = 0x07, /**< архив закрыт, ждёт передачи в ОФД */
    FnStageArchiveClosedFinal = 0x0F, /**< архив ФН закрыт */
} FnStage;

/** Версия ФФД. */
typedef enum {
    FnFfdNone = 0, /**< ФН не активизирован */
    FnFfd1_05 = 1,
    FnFfd1_1 = 3,
    FnFfd1_2 = 4,
    FnFfdUnknown = 0xFF,
} FnFfd;

/** Текущий документ для ОФД (поле ответа 30h). */
typedef enum {
    FnDocNone = 0x00,
    FnDocRegistration = 0x01,
    FnDocSessionOpen = 0x02,
    FnDocReceipt = 0x04,
    FnDocSessionClose = 0x08,
    FnDocFnClose = 0x10,
    FnDocStrictReport = 0x11,
    FnDocRegChangeFnReplace = 0x12,
    FnDocRegChange = 0x13,
    FnDocReceiptCorrection = 0x14,
    FnDocStrictReportCorrection = 0x15,
    FnDocCurrentSettlements = 0x17,
    FnDocUnknown = 0xFF,
} FnDocCode;

/** Биты флагов предупреждений (байт «Флаги предупреждения» ответа 30h). */
enum {
    FnWarnReplaceSoon = 0x01,
    FnWarnResourceSoon = 0x02,
    FnWarnMemoryFull = 0x04,
    FnWarnOfdTimeout = 0x08,
    FnWarnOfdFormatLogic = 0x10,
    FnWarnNeedSetup = 0x20,
    FnWarnOfdCancelled = 0x40,
    FnWarnReserved = 0x80,
};

/** Проверка даты: год 0–99, месяц 1–12, день 1–31. NULL → false. */
bool fn_date_is_valid(const FnDate* date);

/** Проверка даты и времени: дата + час 0–23 + минута 0–59. NULL → false. */
bool fn_datetime_is_valid(const FnDateTime* date_time);

/** Русский текст этапа (статическая строка, без malloc). */
const char* fn_stage_text(FnStage stage);

/** Русский текст версии ФФД: «1.05» / «1.1» / «1.2» / «Не активизирован». */
const char* fn_ffd_text(FnFfd ffd);

/** Русский текст документа для ОФД. */
const char* fn_doc_text(FnDocCode doc);

/**
 * Текст ровно одного описанного флага предупреждения.
 * @return строка для маски 0x01–0x40, иначе NULL
 */
const char* fn_warn_flag_text(uint8_t mask);

/** Неописанные биты флагов (сейчас — только зарезервированный 0x80). */
uint8_t fn_warn_flags_unknown(uint8_t flags);

#ifdef __cplusplus
}
#endif
