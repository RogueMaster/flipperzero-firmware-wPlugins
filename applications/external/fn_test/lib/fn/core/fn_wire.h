/**
 * @file fn_wire.h
 * @brief Канонический байтовый слой провода: LE-числа и CRC16-CCITT.
 *
 * Главное правило: имя функции описывает то, что реально лежит на проводе.
 * В протоколе ФН все многобайтовые поля — little-endian, старший байт CRC
 * не переставляется. См. docs/byte-order.md.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Чтение uint16 LE: p[0] | p[1] << 8. NULL → 0. */
uint16_t fn_wire_u16le_get(const uint8_t* p);

/** Запись uint16 LE: p[0] = v & 0xFF, p[1] = v >> 8. NULL → no-op. */
void fn_wire_u16le_put(uint8_t* p, uint16_t v);

/** Чтение uint32 LE. NULL → 0. */
uint32_t fn_wire_u32le_get(const uint8_t* p);

/** Запись uint32 LE. NULL → no-op. */
void fn_wire_u32le_put(uint8_t* p, uint32_t v);

/**
 * CRC16-CCITT: полином 0x1021, init 0xFFFF, MSB-first.
 * Возвращает ЧИСЛО без перестановки байтов; на проводе пишется через
 * fn_wire_u16le_put(). NULL или len == 0 → 0.
 */
uint16_t fn_wire_crc16_ccitt(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
