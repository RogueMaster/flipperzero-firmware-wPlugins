/**
 * @file fn_frame.h
 * @brief Кодек кадра ФН: сборка и разбор без аллокаций.
 *
 * Формат: 04h | LEN(2, LE) | CMD/ANSWER | DATA... | CRC(2, LE).
 * LEN = data_len + 1, CRC считается по байтам LEN..DATA (START не входит).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FN_FRAME_MAX_LEN 256u
#define FN_FRAME_OVERHEAD 6u
#define FN_FRAME_MAX_PAYLOAD (FN_FRAME_MAX_LEN - FN_FRAME_OVERHEAD)

/** Причина отказа кодека. */
typedef enum {
    FnFrameOk = 0,
    FnFrameNull,
    FnFrameTooShort,
    FnFrameTooLong,
    FnFrameWrongStart,
    FnFrameLengthMismatch,
    FnFrameWrongCrc,
} FnFrameStatus;

/** Разобранный кадр: view внутрь исходного буфера. */
typedef struct {
    uint8_t command; /**< сырой байт CMD для запроса / ANSWER для ответа */
    uint8_t code; /**< command & 0x7F */
    bool has_warning; /**< command & 0x80 */
    const uint8_t* data; /**< указывает внутрь исходного буфера */
    size_t data_len;
} FnFrame;

/**
 * Собрать кадр в буфер вызывающего.
 * @return полный размер кадра или 0 при ошибке; причина — в *status.
 */
size_t fn_frame_build(
    uint8_t command,
    const uint8_t* data,
    size_t data_len,
    uint8_t* out,
    size_t out_cap,
    FnFrameStatus* status);

/** Разобрать кадр без копирования. При ошибке *out не заполняется. */
FnFrameStatus fn_frame_parse(const uint8_t* buf, size_t len, FnFrame* out);

#ifdef __cplusplus
}
#endif
