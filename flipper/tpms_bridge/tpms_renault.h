#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TPMS_RENAULT_FRAME_BYTES 9
#define TPMS_RENAULT_FRAME_BITS  (TPMS_RENAULT_FRAME_BYTES * 8)

/** Номинальная длительность чипа, мкс (~20 kBaud). */
#define TPMS_CHIP_US 50

/** Импульс длиннее — разрыв кадра. */
#define TPMS_MAX_PULSE_US (TPMS_CHIP_US * 8)

/** Хвост преамбулы + sync: "01010101010101010110", 20 чипов. */
#define TPMS_SYNC_PATTERN 0x55556UL
#define TPMS_SYNC_MASK    0xFFFFFUL

typedef struct {
    uint8_t raw[TPMS_RENAULT_FRAME_BYTES];
    uint32_t id;
    uint16_t pressure_raw; /**< умножить на 0.75 -> кПа */
    int16_t temperature_c;
    uint8_t flags;
    uint16_t unknown;
} TpmsRenaultFrame;

typedef struct TpmsRenaultDecoder TpmsRenaultDecoder;

/** Вызывается на каждый кадр с сошедшейся CRC. */
typedef void (*TpmsRenaultCallback)(const uint8_t* raw, void* context);

TpmsRenaultDecoder* tpms_renault_decoder_alloc(TpmsRenaultCallback callback, void* context);
void tpms_renault_decoder_free(TpmsRenaultDecoder* decoder);
void tpms_renault_decoder_reset(TpmsRenaultDecoder* decoder);

/** Скормить декодеру один интервал от радиомодуля. */
void tpms_renault_decoder_feed(TpmsRenaultDecoder* decoder, bool level, uint32_t duration);

/** CRC-8, poly 0x07, init 0x00 (как в rtl_433). */
uint8_t tpms_renault_crc8(const uint8_t* data, size_t len);

/** Разобрать 9 байт в поля. false, если CRC не сошлась. */
bool tpms_renault_parse(const uint8_t* raw, TpmsRenaultFrame* frame);
