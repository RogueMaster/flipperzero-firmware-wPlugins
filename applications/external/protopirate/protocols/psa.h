#pragma once
#include <furi.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <lib/toolbox/manchester_decoder.h>
#include <flipper_format/flipper_format.h>

#include "../defines.h"

#define PSA_PROTOCOL_NAME "PSA"

#define PSA_BF_STATUS_IDLE      0
#define PSA_BF_STATUS_RUNNING   1
#define PSA_BF_STATUS_FOUND     2
#define PSA_BF_STATUS_NOT_FOUND 3
#define PSA_BF_STATUS_CANCELLED 4

typedef struct PsaBfState PsaBfState;
struct PsaBfState {
    volatile uint8_t cancel;
    volatile uint32_t progress_current;
    volatile uint32_t progress_total;
    volatile uint8_t status;
    void (*on_done)(void* context);
    void* on_done_ctx;

    uint32_t key1_low;
    uint32_t key1_high;
    uint16_t key2_low;

    uint8_t decrypted_button;
    uint32_t decrypted_serial;
    uint32_t decrypted_counter;
    uint16_t decrypted_crc;
    uint32_t decrypted_seed;
    uint8_t decrypted_type;
};

void psa_brute_force_run(PsaBfState* state);

int32_t psa_brute_force_thread_entry(void* arg);

bool psa_bf_state_from_flipper_format(PsaBfState* state, FlipperFormat* ff);

typedef struct SubGhzProtocolDecoderPSA SubGhzProtocolDecoderPSA;
typedef struct SubGhzProtocolEncoderPSA SubGhzProtocolEncoderPSA;

extern const SubGhzProtocol psa_protocol;

// Decoder functions
void* subghz_protocol_decoder_psa_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_psa_free(void* context);
void subghz_protocol_decoder_psa_reset(void* context);
void subghz_protocol_decoder_psa_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_psa_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_psa_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
    subghz_protocol_decoder_psa_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_psa_get_string(void* context, FuriString* output);

// Encoder functions (not implemented yet)
void* subghz_protocol_encoder_psa_alloc(SubGhzEnvironment* environment);
void subghz_protocol_encoder_psa_free(void* context);
SubGhzProtocolStatus
    subghz_protocol_encoder_psa_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_encoder_psa_stop(void* context);
LevelDuration subghz_protocol_encoder_psa_yield(void* context);
