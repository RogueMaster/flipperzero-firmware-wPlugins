#pragma once

/**
 * allstar_firefly.h  —  Allstar Firefly 318ALD31K native SubGHz protocol
 *
 * Registers the Allstar Firefly gate remote as a first-class SubGHz protocol,
 * supporting decode from air, save/load of .sub files, and retransmit.
 *
 * Protocol summary (measured from captured remotes):
 *   Carrier   : 318 MHz OOK  (FuriHalSubGhzPresetOok650Async)
 *   Code type : Static 9-bit trinary  (Supertex ED-9 encoder IC)
 *   Frame     : 18 symbols (2 per bit), no separate sync pulse
 *   Repeats   : 20 frames per keypress
 *   Inter-frame gap: ~30 440 µs
 *
 *   Symbol encoding — each bit = two (pulse, gap) pairs:
 *     '+'  ON    : H H  = [4045µs HIGH, 607µs LOW] x2
 *     '-'  OFF   : L L  = [530µs HIGH, 4139µs LOW] x2
 *     '0'  FLOAT : H L  = [4045µs HIGH, 607µs LOW, 530µs HIGH, 4139µs LOW]
 *
 * Save file format:
 *   Filetype: Flipper SubGhz Key File
 *   Version: 1
 *   Frequency: 318000000
 *   Preset: FuriHalSubGhzPresetOok650Async
 *   Protocol: Allstar Firefly
 *   Key: +--000++-
 *
 * To register with the SubGHz app, in protocol_list.c add:
 *   #include "allstar_firefly.h"
 *   &subghz_protocol_allstar_firefly,   (in the protocol array)
 */

#include <lib/subghz/protocols/base.h>

/* ─── Protocol name (must match what's written to .sub files) ─────────────── */
#define SUBGHZ_PROTOCOL_ALLSTAR_FIREFLY_NAME  "Allstar Firefly"

/* ─── Timing constants (from 37-frame analysis of two captures) ───────────── */
#define AF_FREQ              318000000UL  /**< Carrier frequency, Hz            */
#define AF_BIT_COUNT         9u           /**< Trinary DIP bits per frame       */
#define AF_SYM_COUNT         18u          /**< OOK symbols per frame (2/bit)    */

#define AF_LONG_PULSE_US     4045u        /**< H symbol pulse width             */
#define AF_SHORT_PULSE_US    530u         /**< L symbol pulse width             */
#define AF_SHORT_GAP_US      607u         /**< Gap following H symbol           */
#define AF_LONG_GAP_US       4139u        /**< Gap following L symbol           */
#define AF_INTERFRAME_US     30440u       /**< Inter-frame silence              */

/** Pulse duration range for L (short) symbols — outside = noise, reset frame */
#define AF_SHORT_PULSE_MIN   300u
#define AF_SHORT_PULSE_MAX   1200u

/** Pulse duration range for H (long) symbols — outside = noise, reset frame  */
#define AF_LONG_PULSE_MIN    2500u
#define AF_LONG_PULSE_MAX    5500u

/** Gap >= this value marks the end of a frame (real gaps are 26500-30500µs)  */
#define AF_FRAME_THRESH_US   20000u

/** Frames transmitted per keypress                                            */
#define AF_TX_REPEAT         20u

/**
 * TX buffer size: repeat * bits * 2 symbols * 2 values (pulse+gap) per symbol.
 * All repetitions are pre-built into one flat buffer for yield() simplicity.
 */
#define AF_TX_BUF_SIZE       (AF_TX_REPEAT * AF_SYM_COUNT * 2u + 8u)

/* ─── Protocol vtable entries (extern'd for protocol_list.c) ─────────────── */

extern const SubGhzProtocolDecoder subghz_protocol_decoder_allstar_firefly;
extern const SubGhzProtocolEncoder subghz_protocol_encoder_allstar_firefly;
extern const SubGhzProtocol        subghz_protocol_allstar_firefly;
