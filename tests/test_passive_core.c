#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_passive_core.h"

#define HEADER_SIZE 32U
#define ENTRY_SIZE  20U
#define ENTRY_COUNT 36U

typedef struct {
    uint8_t bytes[16384];
    uint32_t size;
    bool fail_reads;
} MemoryFile;

typedef struct {
    bool accept_tone;
    bool accept_voice;
    uint32_t claims;
    uint32_t silences;
    uint32_t tones;
    uint32_t voices;
    uint32_t vibrations_on;
    uint32_t vibrations_off;
    uint32_t releases;
    uint32_t last_voice_rate;
    MfPassivePcmPipe* claimed_pipe;
} FakeServices;

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
    if(file->fail_reads || end < offset || end > file->size) return false;
    memcpy(buffer, file->bytes + offset, length);
    return true;
}

static void make_pack(MemoryFile* file) {
    uint8_t payload[200];
    uint32_t data_offset = HEADER_SIZE + ENTRY_SIZE * ENTRY_COUNT;
    uint32_t offset = data_offset;
    for(size_t i = 0U; i < sizeof(payload); i++) payload[i] = (uint8_t)(i + 16U);
    memset(file, 0, sizeof(*file));
    memcpy(file->bytes, "MFVA", 4U);
    file->bytes[4] = 1U;
    file->bytes[5] = MfPassiveCodecU8;
    put16(file->bytes + 6U, ENTRY_COUNT);
    put32(file->bytes + 8U, 8000U);
    put32(file->bytes + 12U, HEADER_SIZE);
    put32(file->bytes + 16U, offset);
    for(uint8_t id = 0U; id < ENTRY_COUNT; id++) {
        uint8_t* entry = file->bytes + HEADER_SIZE + id * ENTRY_SIZE;
        entry[0] = id;
        put32(entry + 4U, offset);
        put32(entry + 8U, sizeof(payload));
        put32(entry + 12U, sizeof(payload));
        memcpy(file->bytes + offset, payload, sizeof(payload));
        offset += sizeof(payload);
    }
    file->size = offset;
    put32(file->bytes + 20U, file->size);
    put32(
        file->bytes + 24U,
        crc32(0U, file->bytes + HEADER_SIZE, ENTRY_SIZE * ENTRY_COUNT));
    put32(
        file->bytes + 28U,
        crc32(0U, file->bytes + data_offset, file->size - data_offset));
}

static bool fake_claim(
    void* context,
    MfPassiveOutputTarget target,
    uint8_t volume_pct,
    MfPassivePcmPipe* pipe) {
    FakeServices* fake = context;
    CHECK(target == MfPassiveOutputInternal);
    CHECK(volume_pct == 50U);
    fake->claims++;
    fake->claimed_pipe = pipe;
    return true;
}

static bool fake_silence(void* context) {
    FakeServices* fake = context;
    fake->silences++;
    return true;
}

static bool fake_tone(void* context, uint16_t tone_hz) {
    FakeServices* fake = context;
    CHECK(tone_hz == 700U);
    fake->tones++;
    return fake->accept_tone;
}

static bool fake_voice(void* context, uint32_t source_rate_hz) {
    FakeServices* fake = context;
    fake->voices++;
    fake->last_voice_rate = source_rate_hz;
    return fake->accept_voice;
}

static void fake_vibration(void* context, bool enabled) {
    FakeServices* fake = context;
    if(enabled) fake->vibrations_on++;
    else fake->vibrations_off++;
}

static void fake_release(void* context) {
    FakeServices* fake = context;
    fake->releases++;
    fake->claimed_pipe = NULL;
}

static bool fake_command(
    void* context,
    MfPassiveHostCommand command,
    uint32_t value,
    MfPassivePcmPipe* pipe) {
    switch(command) {
    case MfPassiveHostCommandClaim:
        return fake_claim(
            context,
            (MfPassiveOutputTarget)(uint8_t)(value >> 8U),
            (uint8_t)value,
            pipe);
    case MfPassiveHostCommandSilence: return fake_silence(context);
    case MfPassiveHostCommandTone: return fake_tone(context, (uint16_t)value);
    case MfPassiveHostCommandVoice: return fake_voice(context, value);
    case MfPassiveHostCommandVibration:
        fake_vibration(context, value != 0U);
        return true;
    case MfPassiveHostCommandRelease:
        fake_release(context);
        return true;
    default: return false;
    }
}

static MfPassiveHostServices services = {
    .struct_size = sizeof(MfPassiveHostServices),
    .command = fake_command,
};

static void setup(MfPassiveState* state, FakeServices* fake, MemoryFile* file) {
    MfPassiveVoiceIo io = {.context = file, .size = file->size, .read_at = memory_read};
    memset(state, 0, sizeof(*state));
    memset(fake, 0, sizeof(*fake));
    fake->accept_tone = true;
    fake->accept_voice = true;
    services.context = fake;
    state->services = &services;
    state->dit_ms = 10U;
    state->char_gap_ms = 30U;
    state->tone_hz = 700U;
    state->voice_gain_pct = 70U;
    state->mode = 0U;
    state->length_setting = 4U;
    state->prompt_length = 4U;
    state->prompt_len = 4U;
    state->answer_delay_ms = 3000U;
    state->vibrate = 1U;
    state->courtesy_delay_ms = 1000U;
    mf_rx_rng_init(&state->rng, 0x12345678U);
    mf_callsign_gen_init(&state->callsign_gen);
    memcpy(state->callsign.text, "A1A1", 5U);
    state->callsign.text_len = 4U;
    memcpy(state->prompt, "A1A1", 5U);
    CHECK(mf_passive_voice_pack_open_io(&state->pack, &io));
    CHECK(mf_passive_host_claim(state->services, MfPassiveOutputInternal, 700U, 50U, &state->pipe));
    state->audio_claimed = true;
    state->phase = MfPassivePhaseCw;
    state->next_at = 0U;
}

static uint32_t run_cw_to_post(MfPassiveState* state) {
    uint32_t now = 0U;
    while(state->phase == MfPassivePhaseCw) {
        now = state->next_at;
        mf_passive_tick(state, now);
    }
    CHECK(state->phase == MfPassivePhasePostCw);
    return now;
}

static void test_initial_delay(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.phase = MfPassivePhasePrepare;
    state.cw_mark = true;
    state.next_at = 1000U;
    CHECK(fake.tones == 0U);
    mf_passive_tick(&state, 500U);
    CHECK(state.phase == MfPassivePhasePrepare && state.next_at == 1000U);
    mf_passive_tick(&state, 999U);
    CHECK(state.phase == MfPassivePhasePrepare && fake.tones == 0U);
    mf_passive_tick(&state, 1000U);
    CHECK(state.phase == MfPassivePhaseCw && state.cw_mark && fake.tones == 1U);
    CHECK(state.next_at > 1000U);
    mf_passive_leave(&state);
}

static void drain_voice(MfPassiveState* state, uint32_t now) {
    state->pipe.read_pos = state->pipe.write_pos;
    state->pipe.drained = true;
    mf_passive_tick(state, now);
}

static void test_sequence_and_timing(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    MfPassiveResult result;
    uint32_t final_mark;
    uint32_t voice_at;
    char previous[5];

    make_pack(&file);
    setup(&state, &fake, &file);
    final_mark = run_cw_to_post(&state);
    CHECK(state.revealed_count == 0U);
    CHECK(state.next_at == final_mark + 3000U);
    mf_passive_tick(&state, final_mark + 1U);
    CHECK(fake.voices == 0U && state.revealed_count == 0U);
    result = mf_passive_tick(&state, state.next_at - 1U);
    CHECK(!result.redraw && fake.voices == 0U);
    voice_at = state.next_at;
    result = mf_passive_tick(&state, voice_at);
    CHECK(result.redraw && state.phase == MfPassivePhaseVoice);
    CHECK(fake.voices == 1U && fake.last_voice_rate == 8000U && state.revealed_count == 1U);

    state.pipe.underruns = 1U;
    mf_passive_tick(&state, voice_at + 1U);
    CHECK(state.phase == MfPassivePhaseVoice && state.pipe.underruns == 1U);
    state.pipe.underruns = UINT32_MAX;
    mf_passive_tick(&state, voice_at + 1U);
    CHECK(state.phase == MfPassivePhaseError && fake.releases == 1U);
    mf_passive_leave(&state);

    make_pack(&file);
    setup(&state, &fake, &file);
    final_mark = run_cw_to_post(&state);
    voice_at = state.next_at;
    mf_passive_tick(&state, voice_at);
    CHECK(state.phase == MfPassivePhaseVoice);

    drain_voice(&state, voice_at + 2U);
    CHECK(state.phase == MfPassivePhaseBetweenTokens);
    CHECK(state.next_at == voice_at + 102U);
    CHECK(state.revealed_count == 1U && fake.silences > 0U);
    mf_passive_tick(&state, state.next_at - 1U);
    CHECK(fake.voices == 1U && state.revealed_count == 1U);
    result = mf_passive_tick(&state, state.next_at);
    CHECK(result.redraw && state.phase == MfPassivePhaseVoice);
    CHECK(fake.voices == 2U && state.revealed_count == 2U);

    drain_voice(&state, state.next_at + 1U);
    CHECK(state.phase == MfPassivePhaseBetweenTokens);
    result = mf_passive_tick(&state, state.next_at);
    CHECK(result.redraw && fake.voices == 3U && state.revealed_count == 3U);
    drain_voice(&state, state.next_at + 1U);
    CHECK(state.phase == MfPassivePhaseBetweenTokens);
    result = mf_passive_tick(&state, state.next_at);
    CHECK(result.redraw && fake.voices == 4U && state.revealed_count == 4U);

    drain_voice(&state, state.next_at + 1U);
    CHECK(state.phase == MfPassivePhasePostVoice);
    CHECK(state.next_at > voice_at);
    mf_passive_tick(&state, state.next_at - 1U);
    CHECK(fake.vibrations_on == 0U);
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhaseCue && fake.vibrations_on == 1U);
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhasePostCue && fake.vibrations_off == 1U);
    memcpy(previous, state.callsign.text, sizeof(previous));
    result = mf_passive_tick(&state, state.next_at);
    CHECK(result.redraw && state.phase == MfPassivePhaseCw && state.revealed_count == 0U);
    CHECK(memcmp(previous, state.callsign.text, 4U) != 0);
    mf_passive_leave(&state);
    CHECK(fake.releases == 1U);
}

static void test_delayed_tick_and_failures(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.phase = MfPassivePhasePostVoice;
    state.next_at = 5U;
    mf_passive_tick(&state, 5000U);
    CHECK(state.phase == MfPassivePhaseCue && state.next_at == 5030U);
    mf_passive_tick(&state, 5000U);
    CHECK(state.phase == MfPassivePhaseCue);
    mf_passive_tick(&state, 5030U);
    CHECK(state.phase == MfPassivePhasePostCue && state.next_at == 6030U);
    mf_passive_leave(&state);

    make_pack(&file);
    setup(&state, &fake, &file);
    file.fail_reads = true;
    state.phase = MfPassivePhasePostCw;
    state.next_at = 10U;
    mf_passive_tick(&state, 1U);
    CHECK(state.phase == MfPassivePhaseError && state.error == 1U);
    CHECK(fake.releases == 1U && fake.vibrations_off != 0U);
    CHECK(state.pipe.read_pos == 0U && state.pipe.write_pos == 0U);

    make_pack(&file);
    setup(&state, &fake, &file);
    mf_passive_voice_pack_close(&state.pack);
    state.phase = MfPassivePhasePostCw;
    state.next_at = 10U;
    mf_passive_tick(&state, 1U);
    CHECK(state.phase == MfPassivePhaseError && state.error == 1U);
    CHECK(fake.releases == 1U && fake.silences != 0U);

    make_pack(&file);
    setup(&state, &fake, &file);
    fake.accept_voice = false;
    state.phase = MfPassivePhasePostCw;
    state.next_at = 0U;
    mf_passive_tick(&state, 0U);
    CHECK(state.phase == MfPassivePhaseError && state.revealed_count == 0U);
    CHECK(fake.voices == 1U && fake.releases == 1U);
}

static void test_gesture_and_rounds(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    InputEvent event = {.key = InputKeyBack, .type = InputTypeShort};
    char previous[5];

    make_pack(&file);
    setup(&state, &fake, &file);
    event.key = InputKeyUp;
    CHECK(mf_passive_input(&state, &event, 0U).handled);
    CHECK(state.voice_gain_pct == 75U);
    event.type = InputTypeRepeat;
    mf_passive_input(&state, &event, 0U);
    CHECK(state.voice_gain_pct == 80U);
    event.key = InputKeyDown;
    event.type = InputTypeShort;
    mf_passive_input(&state, &event, 0U);
    CHECK(state.voice_gain_pct == 75U);
    state.voice_gain_pct = 100U;
    event.key = InputKeyUp;
    mf_passive_input(&state, &event, 0U);
    CHECK(state.voice_gain_pct == 100U);
    state.voice_gain_pct = 10U;
    event.key = InputKeyDown;
    mf_passive_input(&state, &event, 0U);
    CHECK(state.voice_gain_pct == 10U);
    state.voice_gain_pct = 70U;
    event.key = InputKeyBack;
    CHECK(mf_passive_input(&state, &event, 1U).redraw);
    CHECK(state.back_clicks == 1U);
    CHECK(!mf_passive_input(&state, &event, 701U).request_exit);
    CHECK(mf_passive_input(&state, &event, 1401U).request_exit);
    event.type = InputTypeLong;
    CHECK(!mf_passive_input(&state, &event, 1402U).request_exit && state.back_clicks == 0U);
    event.key = InputKeyOk;
    CHECK(mf_passive_input(&state, &event, 1403U).handled && state.back_clicks == 0U);
    event.key = InputKeyBack;
    event.type = InputTypeShort;
    state.last_back_at = UINT32_MAX - 100U;
    state.back_clicks = 1U;
    CHECK(!mf_passive_input(&state, &event, 50U).request_exit && state.back_clicks == 2U);
    event.key = InputKeyOk;
    event.type = InputTypeLong;
    CHECK(mf_passive_input(&state, &event, 51U).handled && state.back_clicks == 2U);
    event.key = InputKeyBack;
    event.type = InputTypeShort;
    CHECK(mf_passive_input(&state, &event, 52U).request_exit);
    state.back_clicks = 1U;
    state.last_back_at = 2000U;
    CHECK(mf_passive_tick(&state, 2701U).redraw);
    CHECK(state.back_clicks == 0U);

    memcpy(previous, state.callsign.text, sizeof(previous));
    for(uint16_t round = 0U; round < 500U; round++) {
        state.phase = MfPassivePhasePostCue;
        state.next_at = round;
        CHECK(mf_passive_tick(&state, round).redraw);
        CHECK(state.phase == MfPassivePhaseCw && state.revealed_count == 0U);
        CHECK(memcmp(previous, state.callsign.text, 4U) != 0);
        memcpy(previous, state.callsign.text, sizeof(previous));
    }
    state.pipe.read_pos = 9U;
    state.pipe.write_pos = 11U;
    mf_passive_leave(&state);
    CHECK(fake.releases == 1U && state.services == NULL);
}

static void test_repeat_and_vibration_controls(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    uint32_t cue_at;
    uint32_t now;
    uint32_t tones_after_repeat;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.repeat_after_answer = 1U;
    state.phase = MfPassivePhasePostVoice;
    state.next_at = 1000U;
    mf_passive_tick(&state, 1000U);
    CHECK(state.phase == MfPassivePhaseRepeatCw && state.char_index == 0U);
    while(state.phase == MfPassivePhaseRepeatCw) {
        now = state.next_at;
        mf_passive_tick(&state, now);
    }
    CHECK(state.phase == MfPassivePhasePostRepeat && fake.voices == 0U);
    CHECK(state.revealed_count == 0U && strcmp(state.prompt, "A1A1") == 0);
    cue_at = state.next_at;
    tones_after_repeat = fake.tones;
    CHECK(cue_at == now + 1000U);
    mf_passive_tick(&state, cue_at - 1U);
    CHECK(state.phase == MfPassivePhasePostRepeat && fake.tones == tones_after_repeat);
    mf_passive_tick(&state, cue_at);
    CHECK(state.phase == MfPassivePhaseCue && fake.tones == tones_after_repeat + 1U);
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhasePostCue && fake.vibrations_on == 1U);
    mf_passive_leave(&state);

    make_pack(&file);
    setup(&state, &fake, &file);
    state.vibrate = 0U;
    state.phase = MfPassivePhasePostVoice;
    state.next_at = 0U;
    mf_passive_tick(&state, 0U);
    CHECK(state.phase == MfPassivePhaseCue && fake.tones == 1U && fake.vibrations_on == 0U);
    mf_passive_tick(&state, state.next_at);
    CHECK(fake.vibrations_off != 0U);
    mf_passive_leave(&state);
}

static void test_lesson_prompt_bounds_and_delays(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    uint32_t final_mark;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.mode = 1U;
    state.length_setting = 6U;
    state.prompt_length = 6U;
    state.lesson_charset_len = 3U;
    memcpy(state.lesson_charset, "KMU", 3U);
    memcpy(state.prompt, "AAAAAA", 7U);
    state.phase = MfPassivePhasePostCue;
    state.next_at = 0U;
    CHECK(mf_passive_tick(&state, 0U).redraw);
    CHECK(state.prompt_len == 6U && strcmp(state.prompt, "AAAAAA") != 0);
    for(uint8_t i = 0U; i < state.prompt_len; i++)
        CHECK(state.prompt[i] == 'K' || state.prompt[i] == 'M' || state.prompt[i] == 'U');
    mf_passive_leave(&state);

    make_pack(&file);
    setup(&state, &fake, &file);
    state.answer_delay_ms = 1000U;
    final_mark = run_cw_to_post(&state);
    CHECK(state.next_at == final_mark + 1000U);
    state.answer_delay_ms = 5000U;
    state.phase = MfPassivePhaseCw;
    state.prompt[state.prompt_len - 1U] = 'E';
    state.char_index = (uint8_t)(state.prompt_len - 1U);
    state.mark_index = 0U;
    state.cw_mark = true;
    state.next_at = 0U;
    mf_passive_tick(&state, 0U);
    CHECK(state.phase == MfPassivePhasePostCw && state.next_at == 5000U);
    mf_passive_leave(&state);
}

static void test_single_character_lesson_rounds(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    uint32_t final_mark;
    uint32_t voice_at;
    uint32_t repeat_end;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.mode = 1U;
    state.length_setting = 1U;
    state.prompt_length = 1U;
    state.lesson_charset_len = 1U;
    memcpy(state.lesson_charset, "K", 1U);
    memcpy(state.prompt, "K", 2U);
    state.phase = MfPassivePhasePostCue;
    state.next_at = 0U;
    CHECK(mf_passive_tick(&state, 0U).redraw);
    CHECK(state.phase == MfPassivePhaseCw && state.prompt_len == 1U && strcmp(state.prompt, "K") == 0);
    final_mark = run_cw_to_post(&state);
    CHECK(state.next_at == final_mark + 3000U);
    voice_at = state.next_at;
    CHECK(mf_passive_tick(&state, voice_at).redraw);
    CHECK(state.phase == MfPassivePhaseVoice && state.revealed_count == 1U && strcmp(state.prompt, "K") == 0);
    drain_voice(&state, voice_at + 1U);
    CHECK(state.phase == MfPassivePhasePostVoice);
    state.repeat_after_answer = 1U;
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhaseRepeatCw && strcmp(state.prompt, "K") == 0);
    while(state.phase == MfPassivePhaseRepeatCw) {
        repeat_end = state.next_at;
        mf_passive_tick(&state, repeat_end);
    }
    CHECK(state.phase == MfPassivePhasePostRepeat && state.next_at == repeat_end + 1000U);
    mf_passive_tick(&state, state.next_at - 1U);
    CHECK(state.phase == MfPassivePhasePostRepeat);
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhaseCue && state.next_at == repeat_end + 1030U);
    mf_passive_tick(&state, state.next_at - 1U);
    CHECK(state.phase == MfPassivePhaseCue);
    mf_passive_tick(&state, state.next_at);
    CHECK(state.phase == MfPassivePhasePostCue);
    mf_passive_leave(&state);
}

static void test_length_ranges_and_courtesy_off(void) {
    MemoryFile file;
    MfPassiveState state;
    FakeServices fake;
    bool saw_four = false;
    bool saw_five = false;

    make_pack(&file);
    setup(&state, &fake, &file);
    state.length_setting = 7U;
    for(uint16_t round = 0U; round < 200U; round++) {
        state.phase = MfPassivePhasePostCue;
        state.next_at = round;
        CHECK(mf_passive_tick(&state, round).redraw);
        CHECK(state.prompt_len == 4U || state.prompt_len == 5U);
        saw_four |= state.prompt_len == 4U;
        saw_five |= state.prompt_len == 5U;
    }
    CHECK(saw_four && saw_five);

    state.courtesy_delay_ms = 0U;
    state.repeat_after_answer = 0U;
    state.phase = MfPassivePhasePostVoice;
    state.next_at = 500U;
    mf_passive_tick(&state, 500U);
    CHECK(state.phase == MfPassivePhasePostCue);
    CHECK(state.next_at == 1500U);
    CHECK(fake.tones == 200U && fake.vibrations_on == 0U);
    mf_passive_leave(&state);
}

int main(void) {
    CHECK(sizeof(MfPassiveState) <= 2432U);
    test_initial_delay();
    test_sequence_and_timing();
    test_delayed_tick_and_failures();
    test_gesture_and_rounds();
    test_repeat_and_vibration_controls();
    test_lesson_prompt_bounds_and_delays();
    test_single_character_lesson_rounds();
    test_length_ranges_and_courtesy_off();
    printf("test_passive_core: %u checks passed\n", checks);
    return 0;
}
