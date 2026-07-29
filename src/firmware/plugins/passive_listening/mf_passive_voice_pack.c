#include "mf_passive_voice_pack.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <flipper_application/flipper_application.h>
#include <storage/storage.h>
#else
#define __DMB() ((void)0)
#endif

#define MF_PASSIVE_VOICE_HEADER_SIZE 32U
#define MF_PASSIVE_VOICE_ENTRY_SIZE  20U
#define MF_PASSIVE_VOICE_VERSION     1U
#define MF_PASSIVE_VOICE_MAX_SAMPLES 160000UL

static uint16_t mf_passive_voice_le16(const uint8_t* value) {
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8U);
}

static uint32_t mf_passive_voice_le32(const uint8_t* value) {
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8U) | ((uint32_t)value[2] << 16U) |
           ((uint32_t)value[3] << 24U);
}

static uint32_t mf_passive_voice_crc32(uint32_t crc, const uint8_t* data, size_t length) {
    crc = ~crc;
    for(size_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xedb88320UL : 0U);
    }
    return ~crc;
}

static bool mf_passive_voice_read(
    const MfPassiveVoicePack* pack,
    uint32_t offset,
    void* buffer,
    size_t length) {
    uint32_t end;
    if(pack == NULL || pack->io.read_at == NULL || buffer == NULL || length > UINT32_MAX)
        return false;
    end = offset + (uint32_t)length;
    return end >= offset && end <= pack->io.size &&
           pack->io.read_at(pack->io.context, offset, buffer, length);
}

static bool mf_passive_voice_crc_range(
    const MfPassiveVoicePack* pack,
    uint32_t offset,
    uint32_t length,
    uint32_t expected) {
    uint8_t bytes[MF_PASSIVE_VOICE_READ_MAX];
    uint32_t crc = 0U;
    while(length != 0U) {
        uint32_t chunk = length > sizeof(bytes) ? sizeof(bytes) : length;
        if(!mf_passive_voice_read(pack, offset, bytes, chunk)) return false;
        crc = mf_passive_voice_crc32(crc, bytes, chunk);
        offset += chunk;
        length -= chunk;
    }
    return crc == expected;
}

static bool
    mf_passive_voice_token_valid(const MfPassiveVoicePack* pack, const MfPassiveVoiceToken* token) {
    uint32_t end = token->offset + token->length;
    if(token->length == 0U || token->samples == 0U ||
       token->samples > MF_PASSIVE_VOICE_MAX_SAMPLES ||
       token->offset < MF_PASSIVE_VOICE_HEADER_SIZE || end < token->offset || end > pack->io.size)
        return false;
    if(pack->codec_id == MfPassiveCodecS16)
        return (token->length & 1U) == 0U && token->samples == token->length / 2U;
    if(pack->codec_id == MfPassiveCodecU8 || pack->codec_id == MfPassiveCodecMulaw)
        return token->samples == token->length;
    return pack->codec_id == MfPassiveCodecImaAdpcm && token->ima_index <= 88U &&
           token->length == (token->samples + 1U) / 2U;
}

bool mf_passive_voice_char_token(char ch, uint8_t* token) {
    if(ch >= 'A' && ch <= 'Z')
        *token = (uint8_t)(ch - 'A');
    else if(ch >= '0' && ch <= '9')
        *token = (uint8_t)(26U + ch - '0');
    else if(ch == '/')
        *token = 36U;
    else if(ch == '.')
        *token = 37U;
    else if(ch == ',')
        *token = 38U;
    else if(ch == '?')
        *token = 39U;
    else
        return false;
    return true;
}

static uint16_t mf_passive_voice_pipe_count(const MfPassivePcmPipe* pipe) {
    return (uint16_t)((pipe->write_pos - pipe->read_pos) & (MF_PASSIVE_PCM_RING_SAMPLES - 1U));
}

static uint16_t mf_passive_voice_pipe_free(const MfPassivePcmPipe* pipe) {
    return (uint16_t)(MF_PASSIVE_PCM_RING_SAMPLES - 1U - mf_passive_voice_pipe_count(pipe));
}

static size_t mf_passive_voice_decode_to_pipe(
    MfPassiveVoicePack* pack,
    MfPassivePcmPipe* pipe,
    size_t capacity,
    uint8_t gain_pct) {
    size_t written = 0U;
    while(capacity != 0U && pack->source_pos < pack->source_len) {
        uint16_t write = pipe->write_pos;
        size_t contiguous = MF_PASSIVE_PCM_RING_SAMPLES - write;
        size_t source_used = 0U;
        size_t produced;
        if(contiguous > capacity) contiguous = capacity;
        produced = mf_passive_codec_decode(
            &pack->codec,
            pack->source + pack->source_pos,
            pack->source_len - pack->source_pos,
            pipe->samples + write,
            contiguous,
            &source_used);
        pack->source_pos = (uint16_t)(pack->source_pos + source_used);
        if(produced != 0U) {
            if(gain_pct != 100U) {
                for(size_t i = 0U; i < produced; i++) {
                    pipe->samples[write + i] =
                        (int16_t)(((int32_t)pipe->samples[write + i] * gain_pct) / 100);
                }
            }
            __DMB();
            pipe->write_pos = (uint16_t)((write + produced) & (MF_PASSIVE_PCM_RING_SAMPLES - 1U));
        }
        written += produced;
        capacity -= produced;
        if(produced == 0U && source_used == 0U) break;
    }
    return written;
}

static bool mf_passive_voice_validate(MfPassiveVoicePack* pack) {
    uint8_t header[MF_PASSIVE_VOICE_HEADER_SIZE];
    uint8_t entry[MF_PASSIVE_VOICE_ENTRY_SIZE];
    bool seen[MF_PASSIVE_VOICE_TOKEN_COUNT] = {0};
    uint32_t table_offset;
    uint32_t table_length;
    uint32_t table_end;
    uint32_t data_offset;
    uint32_t file_size;
    uint16_t count;

    if(!mf_passive_voice_read(pack, 0U, header, sizeof(header)) ||
       memcmp(header, "MFVA", 4U) != 0 || header[4] != MF_PASSIVE_VOICE_VERSION ||
       header[5] > MfPassiveCodecImaAdpcm)
        return false;
    count = mf_passive_voice_le16(header + 6U);
    pack->sample_rate_hz = mf_passive_voice_le32(header + 8U);
    table_offset = mf_passive_voice_le32(header + 12U);
    data_offset = mf_passive_voice_le32(header + 16U);
    file_size = mf_passive_voice_le32(header + 20U);
    if(count < MF_PASSIVE_VOICE_FIRST_SLICE_TOKENS || count > MF_PASSIVE_VOICE_TOKEN_COUNT ||
       (pack->sample_rate_hz != 8000U && pack->sample_rate_hz != 16000U) ||
       file_size != pack->io.size)
        return false;
    table_length = (uint32_t)count * MF_PASSIVE_VOICE_ENTRY_SIZE;
    table_end = table_offset + table_length;
    if(table_offset < MF_PASSIVE_VOICE_HEADER_SIZE || table_end < table_offset ||
       table_end > data_offset || data_offset > file_size)
        return false;
    pack->codec_id = header[5];
    for(uint16_t i = 0U; i < count; i++) {
        uint8_t id;
        MfPassiveVoiceToken* token;
        if(!mf_passive_voice_read(
               pack, table_offset + i * MF_PASSIVE_VOICE_ENTRY_SIZE, entry, sizeof(entry)))
            return false;
        id = entry[0];
        if(id >= MF_PASSIVE_VOICE_TOKEN_COUNT || seen[id]) return false;
        seen[id] = true;
        token = &pack->tokens[id];
        token->offset = mf_passive_voice_le32(entry + 4U);
        token->length = mf_passive_voice_le32(entry + 8U);
        token->samples = mf_passive_voice_le32(entry + 12U);
        token->ima_predictor = (int16_t)mf_passive_voice_le16(entry + 16U);
        token->ima_index = entry[18];
        if(token->offset < data_offset || !mf_passive_voice_token_valid(pack, token)) return false;
    }
    for(uint8_t id = 0U; id < MF_PASSIVE_VOICE_FIRST_SLICE_TOKENS; id++)
        if(!seen[id]) return false;
    for(uint8_t left = 0U; left < MF_PASSIVE_VOICE_TOKEN_COUNT; left++) {
        if(!seen[left]) continue;
        for(uint8_t right = (uint8_t)(left + 1U); right < MF_PASSIVE_VOICE_TOKEN_COUNT; right++) {
            uint32_t left_end;
            uint32_t right_end;
            if(!seen[right]) continue;
            left_end = pack->tokens[left].offset + pack->tokens[left].length;
            right_end = pack->tokens[right].offset + pack->tokens[right].length;
            if(pack->tokens[left].offset < right_end && pack->tokens[right].offset < left_end)
                return false;
        }
    }
    return mf_passive_voice_crc_range(
               pack, table_offset, table_length, mf_passive_voice_le32(header + 24U)) &&
           mf_passive_voice_crc_range(
               pack, data_offset, file_size - data_offset, mf_passive_voice_le32(header + 28U));
}

bool mf_passive_voice_pack_open_io(MfPassiveVoicePack* pack, const MfPassiveVoiceIo* io) {
    if(pack == NULL || io == NULL || io->read_at == NULL ||
       io->size < MF_PASSIVE_VOICE_HEADER_SIZE)
        return false;
    memset(pack, 0, sizeof(*pack));
    pack->io = *io;
    pack->open = mf_passive_voice_validate(pack);
    pack->error = !pack->open;
    return pack->open;
}

#ifdef MORSE_FLIPPER_FAP
static bool
    mf_passive_voice_storage_read(void* context, uint32_t offset, void* buffer, size_t length) {
    MfPassiveVoicePack* pack = context;
    return pack != NULL && pack->file != NULL && storage_file_seek(pack->file, offset, true) &&
           storage_file_read(pack->file, buffer, length) == length;
}
#endif

bool mf_passive_voice_pack_open_asset(MfPassiveVoicePack* pack) {
#ifdef MORSE_FLIPPER_FAP
    MfPassiveVoiceIo io;
    uint64_t size;
    if(pack == NULL) return false;
    memset(pack, 0, sizeof(*pack));
    pack->storage = furi_record_open(RECORD_STORAGE);
    if(pack->storage == NULL) return false;
    pack->file = storage_file_alloc(pack->storage);
    if(pack->file == NULL || !storage_file_open(
                                 pack->file,
                                 APP_ASSETS_PATH("audio/voice_en_gb_amy_v1.mfa"),
                                 FSAM_READ,
                                 FSOM_OPEN_EXISTING)) {
        mf_passive_voice_pack_close(pack);
        return false;
    }
    size = storage_file_size(pack->file);
    if(size > UINT32_MAX) {
        mf_passive_voice_pack_close(pack);
        return false;
    }
    io = (MfPassiveVoiceIo){
        .context = pack,
        .size = (uint32_t)size,
        .read_at = mf_passive_voice_storage_read,
    };
    pack->io = io;
    pack->open = mf_passive_voice_validate(pack);
    pack->error = !pack->open;
    if(!pack->open) mf_passive_voice_pack_close(pack);
    return pack->open;
#else
    (void)pack;
    return false;
#endif
}

void mf_passive_voice_pack_close(MfPassiveVoicePack* pack) {
    if(pack == NULL) return;
#ifdef MORSE_FLIPPER_FAP
    if(pack->file != NULL) {
        storage_file_close(pack->file);
        storage_file_free(pack->file);
    }
    if(pack->storage != NULL) furi_record_close(RECORD_STORAGE);
#endif
    memset(pack, 0, sizeof(*pack));
}

bool mf_passive_voice_pack_begin(MfPassiveVoicePack* pack, MfPassivePcmPipe* pipe, char ch) {
    MfPassiveVoiceToken* token;
    uint8_t id;
    if(pack == NULL || pipe == NULL || !pack->open || pack->active ||
       !mf_passive_voice_char_token(ch, &id) || pipe->read_pos != pipe->write_pos)
        return false;
    token = &pack->tokens[id];
    if(!mf_passive_codec_begin(
           &pack->codec,
           (MfPassiveCodec)pack->codec_id,
           token->samples,
           token->ima_predictor,
           token->ima_index)) {
        pack->error = true;
        return false;
    }
    pack->payload_at = token->offset;
    pack->payload_end = token->offset + token->length;
    pack->source_pos = 0U;
    pack->source_len = 0U;
    pack->active_token = id;
    pack->active = true;
    pack->eof = false;
    pipe->eof = false;
    pipe->drained = false;
    return true;
}

size_t mf_passive_voice_pack_refill(
    MfPassiveVoicePack* pack,
    MfPassivePcmPipe* pipe,
    uint8_t gain_pct) {
    size_t written = 0U;
    uint16_t free;
    uint32_t read_total = 0U;
    if(pack == NULL || pipe == NULL || !pack->active || pack->error || gain_pct < 10U ||
       gain_pct > 100U)
        return 0U;
    free = mf_passive_voice_pipe_free(pipe);
    while(free != 0U && mf_passive_voice_pipe_count(pipe) < MF_PASSIVE_VOICE_PIPE_HIGH_WATER) {
        uint16_t count = mf_passive_voice_pipe_count(pipe);
        size_t capacity = MF_PASSIVE_VOICE_PIPE_HIGH_WATER - count;
        size_t decoded;
        if(capacity > free) capacity = free;
        decoded = mf_passive_voice_decode_to_pipe(pack, pipe, capacity, gain_pct);
        written += decoded;
        free = (uint16_t)(free - decoded);
        if(mf_passive_codec_finished(&pack->codec)) {
            pack->active = false;
            pack->eof = true;
            __DMB();
            pipe->eof = true;
            break;
        }
        if(pack->source_pos == pack->source_len) {
            uint32_t remaining;
            uint32_t chunk;
            if(pack->payload_at >= pack->payload_end) {
                pack->error = true;
                break;
            }
            if(read_total == MF_PASSIVE_VOICE_READ_MAX) break;
            remaining = pack->payload_end - pack->payload_at;
            chunk = remaining;
            if(chunk > MF_PASSIVE_VOICE_READ_MAX - read_total)
                chunk = MF_PASSIVE_VOICE_READ_MAX - read_total;
            if(chunk == 0U ||
               !mf_passive_voice_read(pack, pack->payload_at, pack->source, chunk)) {
                pack->error = true;
                break;
            }
            pack->payload_at += chunk;
            read_total += chunk;
            pack->source_pos = 0U;
            pack->source_len = (uint16_t)chunk;
            continue;
        }
        if(decoded == 0U) {
            pack->error = true;
            break;
        }
    }
    return written;
}

bool mf_passive_voice_pack_primed(const MfPassiveVoicePack* pack, const MfPassivePcmPipe* pipe) {
    uint16_t count;
    if(pack == NULL || pipe == NULL || pack->error) return false;
    count = mf_passive_voice_pipe_count(pipe);
    return count >= MF_PASSIVE_VOICE_PIPE_PRIME_SAMPLES || (pack->eof && count != 0U);
}

bool mf_passive_voice_pack_eof(const MfPassiveVoicePack* pack) {
    return pack != NULL && pack->eof;
}

bool mf_passive_voice_pack_drained(MfPassiveVoicePack* pack, MfPassivePcmPipe* pipe) {
    if(pack == NULL || pipe == NULL || !pack->eof) return false;
    __DMB();
    return pipe->drained;
}

bool mf_passive_voice_pack_failed(const MfPassiveVoicePack* pack) {
    return pack == NULL || pack->error;
}
