#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Decoded payload bytes kept with a frame. The longest protocol here is
 * 12 bytes; the rest of the app only ever reads raw_len of them. */
#define TPMS_RAW_MAX 16

/** Slicer bits captured after a sync word. The longest capture is the
 * Kia frame: 138 Manchester bits are 276 chips. */
#define TPMS_CAPTURE_MAX_BITS  320
#define TPMS_CAPTURE_MAX_BYTES (TPMS_CAPTURE_MAX_BITS / 8)

/** Which radio configuration a protocol needs. Only one can be active at
 * a time, so the app either picks one or alternates between them. */
typedef enum {
    TpmsModulationFsk,
    TpmsModulationOok,
} TpmsModulation;

/** How the chip stream becomes bits before the sync word is looked for.
 * Mirrors the modulation types of rtl_433: the PCM slicers hand chips
 * over one to one, MANCHESTER_ZEROBIT decodes pairs of chips first. */
typedef enum {
    TpmsSlicerNrz,
    TpmsSlicerManchester,
} TpmsSlicer;

/* Which fields of TpmsFrame the protocol filled in. Sensors differ in
 * what they report, and a missing field must not be drawn as a zero. */
#define TPMS_HAS_PRESSURE 0x01
#define TPMS_HAS_TEMP     0x02
#define TPMS_HAS_BATTERY  0x04 /**< TPMS_BATTERY_LOW is meaningful */
#define TPMS_BATTERY_LOW  0x08
#define TPMS_MOVING       0x10

/** One decoded frame, in units shared by every protocol. */
typedef struct {
    uint8_t protocol; /**< index into tpms_protocols[] */
    uint32_t id;
    int32_t pressure_kpa_x100;
    int16_t temperature_c;
    uint8_t have; /**< TPMS_HAS_* */
    uint8_t flags; /**< protocol specific status bits, as reported */
    uint8_t raw[TPMS_RAW_MAX];
    uint8_t raw_len;
} TpmsFrame;

/** Decode the bits captured after a sync word.
 *
 * `bits` holds nbits slicer bits, most significant bit first, already
 * turned the right way up by the engine. Whatever coding sits inside the
 * payload (Manchester, differential Manchester) is the protocol's own
 * business, exactly as it is in rtl_433.
 *
 * Returns false if the payload is not a valid frame — a wrong checksum,
 * an impossible field, a broken coding.
 */
typedef bool (*TpmsProtocolDecode)(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);

typedef struct {
    const char* id; /**< short name, goes out over the CLI */
    const char* label; /**< what the screen shows */
    uint16_t chip_us; /**< nominal chip duration */
    uint8_t modulation; /**< TpmsModulation */
    uint8_t slicer; /**< TpmsSlicer */
    uint8_t id_digits; /**< hex digits of the sensor id: 6 or 8 */
    uint64_t sync; /**< sync word, right aligned */
    uint8_t sync_bits;
    uint16_t capture_bits; /**< slicer bits to capture after the sync */
    TpmsProtocolDecode decode;
} TpmsProtocol;

extern const TpmsProtocol tpms_protocols[];
extern const uint8_t tpms_protocol_count;

/** Label of a protocol by index, or "?" if the index is out of range. */
const char* tpms_protocol_label(uint8_t index);

/** Short name of a protocol by index, or "?" if the index is out of range. */
const char* tpms_protocol_id(uint8_t index);

/** How many protocols use the given modulation. */
uint8_t tpms_protocol_count_for(TpmsModulation modulation);
