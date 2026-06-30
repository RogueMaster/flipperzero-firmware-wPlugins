#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "ws_generic.h"
#include <lib/subghz/blocks/math.h>

#define WS_PROTOCOL_BL999_NAME "BL999"

typedef struct WSProtocolDecoderBL999 WSProtocolDecoderBL999;
typedef struct WSProtocolEncoderBL999 WSProtocolEncoderBL999;

/* Экспортируем декодер/энкодер и сам объект протокола BL999 */
extern const SubGhzProtocolDecoder ws_protocol_bl999_decoder;
extern const SubGhzProtocolEncoder ws_protocol_bl999_encoder;
extern const SubGhzProtocol ws_protocol_bl999;

/**
 * Выделение (alloc) структуры декодера BL999.
 * @param environment Указатель на SubGhzEnvironment
 * @return Указатель на WSProtocolDecoderBL999
 */
void* ws_protocol_decoder_bl999_alloc(SubGhzEnvironment* environment);

/**
 * Освобождение (free) декодера BL999.
 * @param context Указатель на WSProtocolDecoderBL999
 */
void ws_protocol_decoder_bl999_free(void* context);

/**
 * Сброс декодера BL999.
 * @param context Указатель на WSProtocolDecoderBL999
 */
void ws_protocol_decoder_bl999_reset(void* context);

/**
 * Приём сырых данных (HIGH/LOW) с их длительностями.
 * @param context Указатель на WSProtocolDecoderBL999
 * @param level true=HIGH, false=LOW
 * @param duration длительность уровня, мкс
 */
void ws_protocol_decoder_bl999_feed(void* context, bool level, uint32_t duration);

/**
 * Возврат хэш-суммы (для отсечения повторных пакетов).
 * @param context Указатель на WSProtocolDecoderBL999
 * @return hash 8-битная хэш-сумма
 */
uint8_t ws_protocol_decoder_bl999_get_hash_data(void* context);

/**
 * Сериализация данных BL999 в FlipperFormat.
 * @param context Указатель на WSProtocolDecoderBL999
 * @param flipper_format Указатель на FlipperFormat
 * @param preset Указатель на SubGhzRadioPreset
 * @return статус SubGhzProtocolStatus
 */
SubGhzProtocolStatus ws_protocol_decoder_bl999_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Десериализация данных BL999 из FlipperFormat.
 * @param context Указатель на WSProtocolDecoderBL999
 * @param flipper_format Указатель на FlipperFormat
 * @return статус SubGhzProtocolStatus
 */
SubGhzProtocolStatus
    ws_protocol_decoder_bl999_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Вывод текстового описания полученных данных BL999.
 * @param context Указатель на WSProtocolDecoderBL999
 * @param output FuriString для вывода
 */
void ws_protocol_decoder_bl999_get_string(void* context, FuriString* output);
