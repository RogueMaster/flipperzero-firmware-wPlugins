#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_passive_voice_pack.h"

#define HEADER_SIZE 32U
#define ENTRY_SIZE  20U
#define ENTRY_COUNT 40U

typedef struct {
    uint8_t bytes[65536];
    uint32_t size;
    uint32_t reads;
    size_t largest_read;
} MemoryFile;

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

static void put16(uint8_t* out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8U);
}

static void put32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8U);
    out[2] = (uint8_t)(value >> 16U);
    out[3] = (uint8_t)(value >> 24U);
}

static uint32_t crc32(uint32_t crc, const uint8_t* data, size_t length) {
    crc = ~crc;
    for(size_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xedb88320UL : 0U);
    }
    return ~crc;
}

static bool memory_read(void* context, uint32_t offset, void* buffer, size_t length) {
    MemoryFile* file = context;
    uint32_t end = offset + (uint32_t)length;
    if(end < offset || end > file->size) return false;
    memcpy(buffer, file->bytes + offset, length);
    file->reads++;
    if(length > file->largest_read) file->largest_read = length;
    return true;
}

static MfPassiveVoiceIo memory_io(MemoryFile* file) {
    return (MfPassiveVoiceIo){.context = file, .size = file->size, .read_at = memory_read};
}

static void refresh_crcs(MemoryFile* file) {
    uint32_t table_offset = HEADER_SIZE;
    uint32_t data_offset = HEADER_SIZE + ENTRY_SIZE * ENTRY_COUNT;
    put32(file->bytes + 24U, crc32(0U, file->bytes + table_offset, data_offset - table_offset));
    put32(file->bytes + 28U, crc32(0U, file->bytes + data_offset, file->size - data_offset));
}

static void make_pack(
    MemoryFile* file,
    MfPassiveCodec codec,
    const uint8_t* payload,
    uint32_t payload_length,
    uint32_t samples,
    uint8_t ima_index) {
    uint32_t data_offset = HEADER_SIZE + ENTRY_SIZE * ENTRY_COUNT;
    uint32_t offset = data_offset;
    memset(file, 0, sizeof(*file));
    memcpy(file->bytes, "MFVA", 4U);
    file->bytes[4] = 1U;
    file->bytes[5] = codec;
    put16(file->bytes + 6U, ENTRY_COUNT);
    put32(file->bytes + 8U, 8000U);
    put32(file->bytes + 12U, HEADER_SIZE);
    put32(file->bytes + 16U, data_offset);
    for(uint8_t id = 0U; id < ENTRY_COUNT; id++) {
        uint8_t* entry = file->bytes + HEADER_SIZE + id * ENTRY_SIZE;
        entry[0] = id;
        put32(entry + 4U, offset);
        put32(entry + 8U, payload_length);
        put32(entry + 12U, samples);
        put16(entry + 16U, 0U);
        entry[18] = ima_index;
        memcpy(file->bytes + offset, payload, payload_length);
        offset += payload_length;
    }
    file->size = offset;
    put32(file->bytes + 20U, file->size);
    refresh_crcs(file);
}

static void test_all_codecs(void) {
    static const uint8_t s16[] = {0x00, 0x80, 0x34, 0x12};
    static const uint8_t u8[] = {0U, 128U, 255U, 64U};
    static const uint8_t mulaw[] = {0xffU, 0x7fU, 0U};
    static const uint8_t ima[] = {0x10U, 0x7fU};
    const struct {
        MfPassiveCodec codec;
        const uint8_t* payload;
        uint32_t length;
        uint32_t samples;
    } cases[] = {
        {MfPassiveCodecS16, s16, sizeof(s16), 2U},
        {MfPassiveCodecU8, u8, sizeof(u8), 4U},
        {MfPassiveCodecMulaw, mulaw, sizeof(mulaw), 3U},
        {MfPassiveCodecImaAdpcm, ima, sizeof(ima), 4U},
    };
    for(size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        MemoryFile file;
        MfPassiveVoicePack pack;
        MfPassivePcmPipe pipe = {0};
        MfPassiveVoiceIo io;
        make_pack(&file, cases[i].codec, cases[i].payload, cases[i].length, cases[i].samples, 0U);
        io = memory_io(&file);
        CHECK(mf_passive_voice_pack_open_io(&pack, &io));
        CHECK(mf_passive_voice_pack_begin(&pack, &pipe, 'A'));
        CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 100U) == cases[i].samples);
        CHECK(mf_passive_voice_pack_eof(&pack));
        CHECK(mf_passive_voice_pack_primed(&pack, &pipe));
        CHECK(pipe.write_pos == cases[i].samples);
        CHECK(file.largest_read <= MF_PASSIVE_VOICE_READ_MAX);
        pipe.read_pos = pipe.write_pos;
        CHECK(!mf_passive_voice_pack_drained(&pack, &pipe));
        pipe.drained = true;
        CHECK(mf_passive_voice_pack_drained(&pack, &pipe));
        mf_passive_voice_pack_close(&pack);
    }
}

static void test_wrap_and_pending(void) {
    static const uint8_t payload[] = {0U, 128U, 255U, 64U};
    MemoryFile file;
    MfPassiveVoicePack pack;
    MfPassivePcmPipe pipe = {.read_pos = 1022U, .write_pos = 1022U};
    MfPassiveVoiceIo io;
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 4U, 0U);
    io = memory_io(&file);
    CHECK(mf_passive_voice_pack_open_io(&pack, &io));
    CHECK(mf_passive_voice_pack_begin(&pack, &pipe, 'Z'));
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 100U) == 4U);
    CHECK(pipe.samples[1022] == -32768 && pipe.samples[1023] == 0);
    CHECK(pipe.samples[0] == 32512 && pipe.samples[1] == -16384);
    CHECK(!mf_passive_voice_pack_begin(&pack, &pipe, 'A'));
    pipe.read_pos = pipe.write_pos;
    CHECK(!mf_passive_voice_pack_drained(&pack, &pipe));
    pipe.drained = true;
    CHECK(mf_passive_voice_pack_drained(&pack, &pipe));
    CHECK(mf_passive_voice_pack_begin(&pack, &pipe, 'A'));
    mf_passive_voice_pack_close(&pack);
}

static void test_voice_gain(void) {
    static const uint8_t payload[] = {0U, 128U, 255U, 64U};
    MemoryFile file;
    MfPassiveVoicePack pack;
    MfPassivePcmPipe pipe = {0};
    MfPassiveVoiceIo io;

    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 4U, 0U);
    io = memory_io(&file);
    CHECK(mf_passive_voice_pack_open_io(&pack, &io));
    CHECK(mf_passive_voice_pack_begin(&pack, &pipe, 'A'));
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 0U) == 0U);
    CHECK(pipe.write_pos == 0U);
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 50U) == 4U);
    CHECK(pipe.samples[0] == -16384 && pipe.samples[1] == 0);
    CHECK(pipe.samples[2] == 16256 && pipe.samples[3] == -8192);
    mf_passive_voice_pack_close(&pack);
}

static void test_bounded_refill(void) {
    static uint8_t payload[900];
    MemoryFile file;
    MfPassiveVoicePack pack;
    MfPassivePcmPipe pipe = {0};
    MfPassiveVoiceIo io;
    for(size_t i = 0U; i < sizeof(payload); i++) payload[i] = (uint8_t)i;
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), sizeof(payload), 0U);
    io = memory_io(&file);
    CHECK(mf_passive_voice_pack_open_io(&pack, &io));
    CHECK(mf_passive_voice_pack_begin(&pack, &pipe, '0'));
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 100U) == MF_PASSIVE_VOICE_READ_MAX);
    CHECK(pipe.write_pos == MF_PASSIVE_VOICE_READ_MAX);
    CHECK(!mf_passive_voice_pack_eof(&pack));
    CHECK(!mf_passive_voice_pack_failed(&pack));
    CHECK(file.largest_read <= MF_PASSIVE_VOICE_READ_MAX);
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 100U) ==
          MF_PASSIVE_VOICE_PIPE_HIGH_WATER - MF_PASSIVE_VOICE_READ_MAX);
    CHECK(pipe.write_pos == MF_PASSIVE_VOICE_PIPE_HIGH_WATER);
    CHECK(!mf_passive_voice_pack_failed(&pack));
    pipe.read_pos = pipe.write_pos;
    CHECK(mf_passive_voice_pack_refill(&pack, &pipe, 100U) ==
          sizeof(payload) - MF_PASSIVE_VOICE_PIPE_HIGH_WATER);
    CHECK(mf_passive_voice_pack_eof(&pack));
    CHECK(file.largest_read <= MF_PASSIVE_VOICE_READ_MAX);
    pipe.read_pos = pipe.write_pos;
    CHECK(!mf_passive_voice_pack_drained(&pack, &pipe));
    pipe.drained = true;
    CHECK(mf_passive_voice_pack_drained(&pack, &pipe));
    mf_passive_voice_pack_close(&pack);
}

static void test_rejections(void) {
    static const uint8_t payload[] = {0U, 128U, 255U};
    MemoryFile file;
    MfPassiveVoicePack pack;
    MfPassiveVoiceIo io;
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    io = memory_io(&file);
    CHECK(mf_passive_voice_pack_open_io(&pack, &io));
    mf_passive_voice_pack_close(&pack);

    file.bytes[0] = 'X';
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    file.bytes[4] = 2U;
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    file.bytes[5] = 9U;
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    put32(file.bytes + 8U, 12000U);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));

    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    io.size = HEADER_SIZE - 1U;
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    io = memory_io(&file);
    file.bytes[24] ^= 1U;
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    file.bytes[file.size - 1U] ^= 1U;
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));

    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    put32(file.bytes + HEADER_SIZE + 4U, HEADER_SIZE + ENTRY_SIZE * ENTRY_COUNT - 1U);
    refresh_crcs(&file);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    put32(file.bytes + HEADER_SIZE + 4U, UINT32_MAX - 1U);
    refresh_crcs(&file);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    file.bytes[HEADER_SIZE + ENTRY_SIZE] = 0U;
    refresh_crcs(&file);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 3U, 0U);
    put32(file.bytes + HEADER_SIZE + 12U, 160001U);
    refresh_crcs(&file);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecImaAdpcm, payload, sizeof(payload), 4U, 0U);
    file.bytes[HEADER_SIZE + 18U] = 89U;
    refresh_crcs(&file);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));

    make_pack(&file, MfPassiveCodecU8, payload, sizeof(payload), 2U, 0U);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecS16, payload, 2U, 2U, 0U);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
    make_pack(&file, MfPassiveCodecImaAdpcm, payload, sizeof(payload), 3U, 0U);
    CHECK(!mf_passive_voice_pack_open_io(&pack, &io));
}

static void test_character_tokens(void) {
    uint8_t token = 0U;
    for(char ch = 'A'; ch <= 'Z'; ch++) {
        CHECK(mf_passive_voice_char_token(ch, &token));
        CHECK(token == (uint8_t)(ch - 'A'));
    }
    for(char ch = '0'; ch <= '9'; ch++) {
        CHECK(mf_passive_voice_char_token(ch, &token));
        CHECK(token == (uint8_t)(26U + ch - '0'));
    }
    CHECK(mf_passive_voice_char_token('/', &token) && token == 36U);
    CHECK(mf_passive_voice_char_token('.', &token) && token == 37U);
    CHECK(mf_passive_voice_char_token(',', &token) && token == 38U);
    CHECK(mf_passive_voice_char_token('?', &token) && token == 39U);
    CHECK(!mf_passive_voice_char_token('=', &token));
}

int main(void) {
    test_all_codecs();
    test_wrap_and_pending();
    test_voice_gain();
    test_bounded_refill();
    test_rejections();
    test_character_tokens();
    printf("test_passive_voice_pack: %u checks passed; state=%u bytes\n", checks, (unsigned)sizeof(MfPassiveVoicePack));
    return 0;
}
