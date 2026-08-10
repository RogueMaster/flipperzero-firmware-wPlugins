#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TPMS_RENAULT_FRAME_BYTES 9
#define TPMS_RENAULT_FRAME_BITS  (TPMS_RENAULT_FRAME_BYTES * 8)

/** Nominal chip duration, us (~20 kBaud). */
#define TPMS_CHIP_US 50

/** A pulse longer than this breaks the frame. */
#define TPMS_MAX_PULSE_US (TPMS_CHIP_US * 8)

/** Preamble tail plus sync word: "01010101010101010110", 20 chips. */
#define TPMS_SYNC_PATTERN 0x55556UL
#define TPMS_SYNC_MASK    0xFFFFFUL

typedef struct {
    uint8_t raw[TPMS_RENAULT_FRAME_BYTES];
    uint32_t id;
    uint16_t pressure_raw; /**< multiply by 0.75 to get kPa */
    int16_t temperature_c;
    uint8_t flags;
    uint16_t unknown;
} TpmsRenaultFrame;

typedef struct TpmsRenaultDecoder TpmsRenaultDecoder;

/** Called for every frame with a matching CRC. */
typedef void (*TpmsRenaultCallback)(const uint8_t* raw, void* context);

TpmsRenaultDecoder* tpms_renault_decoder_alloc(TpmsRenaultCallback callback, void* context);
void tpms_renault_decoder_free(TpmsRenaultDecoder* decoder);
void tpms_renault_decoder_reset(TpmsRenaultDecoder* decoder);

/** Feed one interval from the radio into the decoder. */
void tpms_renault_decoder_feed(TpmsRenaultDecoder* decoder, bool level, uint32_t duration);

/** CRC-8, poly 0x07, init 0x00 (same as in rtl_433). */
uint8_t tpms_renault_crc8(const uint8_t* data, size_t len);

/** Parse 9 bytes into fields. Returns false if the CRC does not match. */
bool tpms_renault_parse(const uint8_t* raw, TpmsRenaultFrame* frame);
