#pragma once
/*
 * 16-bit M-field -> three ASCII letters.
 * EN 13757-3 §5.6: each letter = (5 bits) + 64. Bytes are little-endian.
 * E.g. M-field = 0x2C2D -> letters 'K','A','M' -> "KAM" (Kamstrup).
 */

#include <stdint.h>

void wmbus_manuf_decode(uint16_t m_field, char out[4]);

/* Best-effort short company name lookup (returns NULL if unknown). */
const char* wmbus_manuf_name(uint16_t m_field);
