/**
 * @file fn_error.h
 * @brief Каталог кодов ответа ФН (Протокол v1.2.05, раздел 3.4).
 *
 * Тексты — статические строки на русском, без malloc.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Коды ответа ФН: значения из ответа (ANSWER & 0x7F). */
typedef enum {
    FNOk = 0x00,
    FNErrorUnknownCMD = 0x01,
    FNErrorWrongState = 0x02,
    FNErrorFailure = 0x03,
    FNErrorFailureKS = 0x04,
    FNErrorCMDParamsNotMatchFNEndDate = 0x05,
    FNErrorWrongDateTime = 0x07,
    FNErrorNoRequestedData = 0x08,
    FNErrorWrongParamValue = 0x09,
    FNErrorWrongCommandFfd11 = 0x0A,
    FNErrorForbiddenAttribute = 0x0B,
    FNErrorDuplicateData = 0x0C,
    FNErrorNoDataForAccounting = 0x0D,
    FNErrorTooManyPositions = 0x0E,
    FNErrorTlvTooBig = 0x10,
    FNErrorNoTransport = 0x11,
    FNErrorResourceExhausted = 0x12,
    FNErrorResourceLimited = 0x14,
    FNErrorSessionTooLong = 0x16,
    FNErrorWrongTimeInterval = 0x17,
    FNErrorWrongAttribute = 0x18,
    FNErrorAttributeNotRegistered = 0x19,
    FNErrorWrongAnswer = 0x20,
    FNErrorKmUpdateNegative = 0x23,
    FNErrorKmUpdateUnknown = 0x24,
    FNErrorKmUpdateRepeat = 0x30,
    FNErrorMarkingForbidden = 0x32,
    FNErrorMarkingSequence = 0x33,
    FNErrorMarkingBlocked = 0x34,
    FNErrorKmTableOverflow = 0x35,
    FNErrorKmKeysExpired = 0x36,
    FNErrorTlvMissingAttribute = 0x3C,
    FNErrorKmNotChecked = 0x3E,
} FNError;

/** Код входит в каталог протокола. */
bool fn_error_is_known(FNError code);

/** Короткий русский текст; для неизвестного — «Неизвестный код ответа ФН». */
const char* fn_error_title(FNError code);

/** Подсказка; пустая строка, если ее нет. Никогда не NULL. */
const char* fn_error_hint(FNError code);

#ifdef __cplusplus
}
#endif
