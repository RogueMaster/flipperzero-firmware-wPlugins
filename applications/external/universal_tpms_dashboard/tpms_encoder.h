#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "helpers/tpms_edit_mask.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TPMS_TX_MAX_PULSES 512U

typedef enum {
    OOK_PULSE_PCM = 0,
    OOK_PULSE_MANCHESTER_ZEROBIT,
    FSK_PULSE_PCM,
    FSK_PULSE_MANCHESTER_ZEROBIT,
} modulation_t;

typedef enum {
    TpmsRadioPresetAuto = 0,
    TpmsRadioPresetFSK = 1,
    TpmsRadioPresetOOK = 2,
    TpmsRadioPresetGFSK = 3,
} TpmsRadioPreset;

typedef enum {
    TpmsWakeProfileNone = 0,
    TpmsWakeProfileFord5A5A = 1,
    TpmsWakeProfileVDOFCA615E = 2,
    TpmsWakeProfileAutoFordVDO = 3,
} TpmsWakeProfile;

typedef enum {
    TpmsEncoderNone = 0,
    TpmsEncoderRenault,
    TpmsEncoderToyota,
    TpmsEncoderSchrader,
    TpmsEncoderSchraderEG53MA4,
    TpmsEncoderCitroen,
    TpmsEncoderFord,
    TpmsEncoderBMWGen23,
    TpmsEncoderElantra2012,
    TpmsEncoderHyundaiVDO,
    TpmsEncoderTruck,
    TpmsEncoderRenault0435R,
    TpmsEncoderHondaTRW,
    TpmsEncoderPorsche,
    TpmsEncoderKia,
    TpmsEncoderPMV107J,
    TpmsEncoderTG1C,
    TpmsEncoderQ85,
    TpmsEncoderMercedesSprinter,
    TpmsEncoderCount,
} TpmsEncoderKind;

typedef struct {
    uint32_t duration_us;
    bool level;
} TpmsPulse;

typedef struct {
    TpmsEncoderKind kind;
    uint32_t id;
    float pressure_kpa;
    float temperature_c;
    uint32_t flags;
    uint8_t temperature_raw_f;
    bool temperature_raw_f_valid;
    uint32_t frequency_hz;
    /* Physical RX preset used for the captured frame. AUTO means unknown/legacy.
       This is the receiver setting, not proof of the transmitter's exact modulation. */
    uint8_t radio_preset;
    /* Optional frame RSSI retained by ordinary RX/autosave profiles. */
    int16_t rssi_dbm;
    bool rssi_valid;
    /* Optional correlation with the last Ford-family LF wake cycle. */
    uint8_t wake_profile;
    bool wake_profile_valid;
    uint8_t repeat_count;
    uint8_t edit_mask;

    /* TPMS 2.3 lossless source payload. Up to 128 decoded bits covers all
     * eighteen currently registered protocols. */
    uint8_t raw_payload[16];
    uint8_t raw_payload_size;
    uint8_t raw_payload_bits;
    bool raw_payload_valid;
} TpmsEditValues;

typedef struct {
    float pressure_kpa;
    float temperature_c;
    uint32_t flags;
} TpmsEncodedValues;

typedef struct {
    TpmsPulse pulses[TPMS_TX_MAX_PULSES];
    size_t pulse_count;
    uint8_t bytes[32];
    size_t byte_count;
    size_t logical_bits;
    uint32_t frequency_hz;
    modulation_t modulation;
    bool overflowed;
} TpmsTxWave;

const char* tpms_encoder_name(TpmsEncoderKind kind);
const char* tpms_encoder_display_name(TpmsEncoderKind kind);
const char* tpms_protocol_display_name(const char* internal_name);
const char* tpms_protocol_list_name(const char* internal_name);
const char* tpms_radio_preset_name(uint8_t preset);
uint8_t tpms_radio_preset_from_name(const char* preset_name);
const char* tpms_wake_profile_name(uint8_t profile);
const char* tpms_wake_profile_short_name(uint8_t profile);
TpmsEncoderKind tpms_encoder_for_model(const char* model);
bool tpms_encoder_build(const TpmsEditValues* values, TpmsTxWave* wave);
bool tpms_encoder_quantize(const TpmsEditValues* values, TpmsEncodedValues* encoded);
size_t tpms_encoder_test_count(void);
TpmsEncoderKind tpms_encoder_test_kind(size_t index);
void tpms_encoder_default_values(TpmsEncoderKind kind, TpmsEditValues* values);
bool tpms_encoder_has_temperature(TpmsEncoderKind kind);
bool tpms_encoder_flags_editable(TpmsEncoderKind kind);
bool tpms_encoder_is_experimental(TpmsEncoderKind kind);
bool tpms_encoder_is_ook(TpmsEncoderKind kind);
float tpms_encoder_pressure_step_kpa(TpmsEncoderKind kind);

#ifdef __cplusplus
}
#endif
