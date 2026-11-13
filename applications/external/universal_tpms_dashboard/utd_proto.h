/*
 * utd_proto - ponte tra Universal TPMS Dashboard e i codec di TPMS-Editor-EN
 * (core ProtoView di Salvatore Sanfilippo + decoder/encoder di altruista86).
 *
 * L'app principale non include mai proto6_core.h: parla solo con questa API.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tpms_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UTD_RAW_MAX 16U
#define UTD_ID_MAX  4U

/* Profili radio: 0..2 = 315 MHz FSK/OOK/GFSK, 3..5 = 433.92 MHz FSK/OOK/GFSK */
#define UTD_PROFILES 6U

/* Un frame decodificato da uno dei decoder. */
typedef struct {
    TpmsEncoderKind kind;
    uint32_t id;
    uint8_t id_len; /* byte significativi dell'ID (3 o 4) */
    float kpa;
    int16_t temp_c;
    bool temp_valid;
    uint32_t flags;
    uint8_t temp_f_raw;
    bool temp_f_valid;
    uint8_t raw[UTD_RAW_MAX];
    uint8_t raw_size;
    uint8_t raw_bits;
    bool raw_valid;
} UtdFrame;

/* Dati completi per costruire e trasmettere un frame. */
typedef struct {
    TpmsEncoderKind kind;
    uint32_t id;
    float kpa;
    int16_t temp_c;
    uint32_t flags;
    uint8_t temp_f_raw;
    bool temp_f_valid;
    uint8_t raw[UTD_RAW_MAX];
    uint8_t raw_size;
    uint8_t raw_bits;
    bool raw_valid;
    uint8_t edit_mask; /* TPMS_EDIT_* dei campi da riscrivere sul payload catturato */
    uint8_t profile;   /* profilo radio 0..5 */
    uint8_t repeats;
} UtdTx;

bool utd_init(void);
void utd_deinit(void);

/* ---- RX ---------------------------------------------------------------- */
/* Azzera lo scanner e imposta la famiglia di decoder (OOK o FSK). */
void utd_rx_reset(bool ook);
void utd_rx_feed(bool level, uint32_t duration_us);
/* Da chiamare spesso: true se e' stato decodificato un frame (in *out). */
bool utd_rx_scan(UtdFrame* out);

/* ---- TX ---------------------------------------------------------------- */
bool utd_tx_allowed(uint8_t profile);
bool utd_transmit(const UtdTx* tx);

/* ---- informazioni sui protocolli ---------------------------------------- */
uint8_t utd_kind_id_len(TpmsEncoderKind kind);
bool utd_kind_is_ook(TpmsEncoderKind kind);
uint8_t utd_kind_default_profile(TpmsEncoderKind kind);
/* 0 = verificato, 1 = TEST, 2 = EXP */
uint8_t utd_kind_maturity(TpmsEncoderKind kind);
const char* utd_kind_short(TpmsEncoderKind kind);
/* passo di pressione in kPa (>=1) per i tasti su/giu' */
uint16_t utd_kind_step_kpa(TpmsEncoderKind kind);

/* ---- profili radio ------------------------------------------------------- */
uint32_t utd_profile_freq(uint8_t profile);
bool utd_profile_is_ook(uint8_t profile);
const uint8_t* utd_profile_preset(uint8_t profile);
const char* utd_profile_name(uint8_t profile);
/* ordine di scansione per l'auto-rilevamento (n = 0..UTD_PROFILES-1) */
uint8_t utd_profile_auto_order(uint8_t n);

#ifdef __cplusplus
}
#endif
