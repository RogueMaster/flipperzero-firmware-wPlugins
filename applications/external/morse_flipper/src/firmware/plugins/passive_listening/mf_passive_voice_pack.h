#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mf_passive_audio_pipe.h"
#include "mf_passive_codec.h"

#define MF_PASSIVE_VOICE_TOKEN_COUNT        40U
#define MF_PASSIVE_VOICE_FIRST_SLICE_TOKENS 36U
#define MF_PASSIVE_VOICE_READ_MAX           512U
#define MF_PASSIVE_VOICE_PIPE_HIGH_WATER    960U
#define MF_PASSIVE_VOICE_PIPE_PRIME_SAMPLES 128U

_Static_assert(
    (MF_PASSIVE_PCM_RING_SAMPLES & (MF_PASSIVE_PCM_RING_SAMPLES - 1U)) == 0U,
    "passive PCM ring size must be a power of two");
_Static_assert(
    MF_PASSIVE_VOICE_PIPE_HIGH_WATER < MF_PASSIVE_PCM_RING_SAMPLES,
    "passive voice high-water mark must leave one ring slot free");
_Static_assert(
    MF_PASSIVE_VOICE_PIPE_PRIME_SAMPLES <= MF_PASSIVE_VOICE_PIPE_HIGH_WATER,
    "passive voice prime must fit below the high-water mark");

typedef struct Storage Storage;
typedef struct File File;

typedef struct {
    void* context;
    uint32_t size;
    bool (*read_at)(void* context, uint32_t offset, void* buffer, size_t length);
} MfPassiveVoiceIo;

typedef struct {
    uint32_t offset;
    uint32_t length;
    uint32_t samples;
    int16_t ima_predictor;
    uint8_t ima_index;
} MfPassiveVoiceToken;

typedef struct {
    MfPassiveVoiceIo io;
    Storage* storage;
    File* file;
    MfPassiveVoiceToken tokens[MF_PASSIVE_VOICE_TOKEN_COUNT];
    MfPassiveCodecState codec;
    uint32_t sample_rate_hz;
    uint32_t payload_at;
    uint32_t payload_end;
    uint16_t source_pos;
    uint16_t source_len;
    uint8_t codec_id;
    uint8_t active_token;
    uint8_t source[MF_PASSIVE_VOICE_READ_MAX];
    bool open;
    bool active;
    bool eof;
    bool error;
} MfPassiveVoicePack;

bool mf_passive_voice_pack_open_io(MfPassiveVoicePack* pack, const MfPassiveVoiceIo* io);
bool mf_passive_voice_char_token(char ch, uint8_t* token);
bool mf_passive_voice_pack_open_asset(MfPassiveVoicePack* pack);
void mf_passive_voice_pack_close(MfPassiveVoicePack* pack);
bool mf_passive_voice_pack_begin(MfPassiveVoicePack* pack, MfPassivePcmPipe* pipe, char ch);
size_t mf_passive_voice_pack_refill(
    MfPassiveVoicePack* pack,
    MfPassivePcmPipe* pipe,
    uint8_t gain_pct);
bool mf_passive_voice_pack_primed(const MfPassiveVoicePack* pack, const MfPassivePcmPipe* pipe);
bool mf_passive_voice_pack_eof(const MfPassiveVoicePack* pack);
bool mf_passive_voice_pack_drained(MfPassiveVoicePack* pack, MfPassivePcmPipe* pipe);
bool mf_passive_voice_pack_failed(const MfPassiveVoicePack* pack);
