#pragma once

#include "tpms_bits.h"
#include "tpms_protocol.h"

/* Every decoder ported from rtl_433 lives in one of the tpms_proto_*.c
 * files, one file per driver in rtl_433/src/devices, and is listed here.
 * They are kept as close to the originals as fixed point arithmetic
 * allows, so that the two can be read side by side. */

bool tpms_decode_renault(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_renault_0435r(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_citroen(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_ford(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_toyota(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_abarth124(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_egq_q85(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_jansite(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_jansite_solar(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_jansite_ty588(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_porsche(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_truck(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_hyundai_vdo(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_elantra2012(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_honda(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_kia(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_sefis_m3(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_airpuxem(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_bmw_g3(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_ave(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_pmv107j(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_nissan(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_bmw(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_mercedes_benz(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_steelmate(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_trw(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_schrader(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_schrader_eg53ma4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_schrader_smd3ma4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_schrader_mrxbc5a4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_schrader_motorcycle(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_gm(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_gear_hive(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_tyreguard400(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_smartire(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_eezrv(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_jansite_ty468(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
bool tpms_decode_imars_t240(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame);
