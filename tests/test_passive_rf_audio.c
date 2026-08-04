#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mf_passive_rf_audio.h"
#include "mf_passive_voice_pack.h"

#define TEST_FREQUENCY_HZ 433160000U

typedef enum {
    CallIdle = 1,
    CallLoadPreset,
    CallSetFrequency,
    CallDataGpio,
    CallAsyncStart,
    CallAsyncStop,
    CallSleep,
    CallLedOn,
    CallLedOff,
    CallInsomniaEnter,
    CallInsomniaExit,
} FakeCall;

typedef struct {
    FakeCall calls[128];
    uint32_t probes[64];
    size_t call_count;
    size_t probe_count;
    const uint8_t* preset;
    size_t preset_size;
    uint32_t invalid_hz;
    uint32_t denied_hz;
    uint32_t last_frequency_hz;
    MfPassiveRfAudio* async_context;
    unsigned insomnia_enters;
    unsigned insomnia_exits;
    unsigned async_starts;
    unsigned async_stops;
    bool preset_ok;
    uint32_t tuned_frequency_hz;
    bool async_start_ok;
    bool led_on;
    bool sleeping;
} FakeHardware;

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

static void record(FakeHardware* fake, FakeCall call) {
    assert(fake->call_count < sizeof(fake->calls) / sizeof(fake->calls[0]));
    fake->calls[fake->call_count++] = call;
}

static bool fake_frequency_valid(void* context, uint32_t frequency_hz) {
    FakeHardware* fake = context;
    assert(fake->probe_count < sizeof(fake->probes) / sizeof(fake->probes[0]));
    fake->probes[fake->probe_count++] = frequency_hz;
    return frequency_hz != fake->invalid_hz;
}

static bool fake_frequency_allowed(void* context, uint32_t frequency_hz) {
    FakeHardware* fake = context;
    assert(fake->probe_count < sizeof(fake->probes) / sizeof(fake->probes[0]));
    fake->probes[fake->probe_count++] = frequency_hz;
    return frequency_hz != fake->denied_hz;
}

static void fake_idle(void* context) {
    record(context, CallIdle);
}

static bool fake_load_preset(void* context, const uint8_t* preset, size_t size) {
    FakeHardware* fake = context;
    fake->preset = preset;
    fake->preset_size = size;
    record(fake, CallLoadPreset);
    return fake->preset_ok;
}

static uint32_t fake_set_frequency(void* context, uint32_t frequency_hz) {
    FakeHardware* fake = context;
    fake->last_frequency_hz = frequency_hz;
    record(fake, CallSetFrequency);
    return fake->tuned_frequency_hz;
}

static void fake_data_gpio(void* context) {
    record(context, CallDataGpio);
}

static bool fake_async_start(void* context, MfPassiveRfAudio* audio) {
    FakeHardware* fake = context;
    fake->async_context = audio;
    fake->async_starts++;
    record(fake, CallAsyncStart);
    return fake->async_start_ok;
}

static void fake_async_stop(void* context) {
    FakeHardware* fake = context;
    fake->async_stops++;
    record(fake, CallAsyncStop);
}

static void fake_sleep(void* context) {
    FakeHardware* fake = context;
    fake->sleeping = true;
    record(fake, CallSleep);
}

static void fake_led(void* context, bool on) {
    FakeHardware* fake = context;
    fake->led_on = on;
    record(fake, on ? CallLedOn : CallLedOff);
}

static void fake_insomnia_enter(void* context) {
    FakeHardware* fake = context;
    fake->insomnia_enters++;
    record(fake, CallInsomniaEnter);
}

static void fake_insomnia_exit(void* context) {
    FakeHardware* fake = context;
    fake->insomnia_exits++;
    record(fake, CallInsomniaExit);
}

static const MfPassiveRfHardwareOps fake_ops = {
    .frequency_valid = fake_frequency_valid,
    .frequency_allowed = fake_frequency_allowed,
    .radio_idle = fake_idle,
    .load_preset = fake_load_preset,
    .set_frequency_and_path = fake_set_frequency,
    .data_gpio_input = fake_data_gpio,
    .async_start = fake_async_start,
    .async_stop = fake_async_stop,
    .radio_sleep = fake_sleep,
    .set_led = fake_led,
    .insomnia_enter = fake_insomnia_enter,
    .insomnia_exit = fake_insomnia_exit,
};

static void setup(MfPassiveRfAudio* audio, MfPassivePcmPipe* pipe, FakeHardware* fake) {
    memset(pipe, 0, sizeof(*pipe));
    memset(fake, 0, sizeof(*fake));
    fake->preset_ok = true;
    fake->tuned_frequency_hz = TEST_FREQUENCY_HZ;
    fake->async_start_ok = true;
    mf_passive_rf_audio_init(audio, &fake_ops, fake, pipe);
}

static bool preset_has(const FakeHardware* fake, uint8_t reg, uint8_t value) {
    for(size_t i = 0U; i + 1U < fake->preset_size; i += 2U)
        if(fake->preset[i] == reg && fake->preset[i + 1U] == value) return true;
    return false;
}

static void
    prepare_and_start(MfPassiveRfAudio* audio, MfPassivePcmPipe* pipe, FakeHardware* fake) {
    setup(audio, pipe, fake);
    CHECK(mf_passive_rf_audio_prepare(audio, TEST_FREQUENCY_HZ));
    CHECK(mf_passive_rf_audio_start_burst(audio));
}

static void test_duration_cycle(void) {
    MfPassiveRfAudio audio = {0};
    static const uint8_t expected[] = {21U, 21U, 21U, 21U, 21U, 20U};
    uint64_t sum = 0U;

    for(size_t i = 0U; i < sizeof(expected); i++) {
        MfPassiveRfPulse pulse = mf_passive_rf_audio_next_pulse(&audio);
        CHECK(pulse.duration_us == expected[i]);
        sum += pulse.duration_us;
    }
    CHECK(sum == 125U);
    sum = 0U;
    memset(&audio, 0, sizeof(audio));
    for(uint32_t i = 0U; i < MF_PASSIVE_RF_PDM_HZ; i++)
        sum += mf_passive_rf_audio_next_pulse(&audio).duration_us;
    CHECK(sum == 1000000U);
}

static void test_voice_pdm_and_gain(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;
    static const bool expected[] = {true, false, true, false, true, false};

    prepare_and_start(&audio, &pipe, &fake);
    pipe.samples[0] = 10000;
    pipe.samples[1] = -10000;
    pipe.write_pos = 2U;
    pipe.eof = true;
    CHECK(mf_passive_rf_audio_start_voice(&audio));
    CHECK(audio.sample == 15000 && pipe.read_pos == 1U);
    for(size_t i = 0U; i < sizeof(expected) / sizeof(expected[0]); i++)
        CHECK(mf_passive_rf_audio_next_pulse(&audio).level == expected[i]);
    CHECK(pipe.read_pos == 2U && pipe.drained);

    mf_passive_rf_audio_pause(&audio);
    pipe.read_pos = 0U;
    pipe.write_pos = 1U;
    pipe.eof = false;
    pipe.samples[0] = 30000;
    CHECK(mf_passive_rf_audio_start_burst(&audio));
    CHECK(mf_passive_rf_audio_start_voice(&audio));
    CHECK(audio.sample == INT16_MAX);
    mf_passive_rf_audio_pause(&audio);
    pipe.read_pos = 0U;
    pipe.write_pos = 1U;
    pipe.samples[0] = -30000;
    CHECK(mf_passive_rf_audio_start_burst(&audio));
    CHECK(mf_passive_rf_audio_start_voice(&audio));
    CHECK(audio.sample == INT16_MIN);
    mf_passive_rf_audio_release(&audio);
}

static void test_voice_pacing_wrap_and_eof(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;

    prepare_and_start(&audio, &pipe, &fake);
    pipe.samples[1023] = 1000;
    pipe.samples[0] = 2000;
    pipe.read_pos = 1023U;
    pipe.write_pos = 1U;
    pipe.eof = true;
    CHECK(mf_passive_rf_audio_start_voice(&audio));
    CHECK(pipe.read_pos == 0U && audio.sample == 1500);
    mf_passive_rf_audio_next_pulse(&audio);
    mf_passive_rf_audio_next_pulse(&audio);
    CHECK(pipe.read_pos == 0U && !pipe.drained);
    mf_passive_rf_audio_next_pulse(&audio);
    CHECK(pipe.read_pos == 1U && audio.sample == 3000 && !pipe.drained);
    mf_passive_rf_audio_next_pulse(&audio);
    mf_passive_rf_audio_next_pulse(&audio);
    CHECK(!pipe.drained);
    mf_passive_rf_audio_next_pulse(&audio);
    CHECK(pipe.drained && audio.source == MfPassiveRfSourceSilence);
    mf_passive_rf_audio_release(&audio);
}

static void test_underrun_and_silence(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;

    prepare_and_start(&audio, &pipe, &fake);
    pipe.samples[0] = 12000;
    pipe.write_pos = 1U;
    CHECK(mf_passive_rf_audio_start_voice(&audio));
    for(uint8_t i = 0U; i < 3U; i++)
        mf_passive_rf_audio_next_pulse(&audio);
    CHECK(pipe.underruns == 1U && audio.sample == 0);
    pipe.underruns = UINT32_MAX;
    for(uint8_t i = 0U; i < 3U; i++)
        mf_passive_rf_audio_next_pulse(&audio);
    CHECK(pipe.underruns == UINT32_MAX && audio.sample == 0);
    audio.sample = 12345;
    mf_passive_rf_audio_set_silence(&audio);
    CHECK(audio.sample == 0 && audio.source == MfPassiveRfSourceSilence);
    mf_passive_rf_audio_next_pulse(&audio);
    CHECK(audio.sample == 0);
    mf_passive_rf_audio_release(&audio);
}

static unsigned tone_transitions(uint16_t tone_hz) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;
    bool previous;
    unsigned transitions = 0U;

    prepare_and_start(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_start_tone(&audio, tone_hz, 1000U));
    previous = audio.tone_positive;
    for(uint32_t i = 0U; i < MF_PASSIVE_RF_PDM_HZ; i++) {
        mf_passive_rf_audio_next_pulse(&audio);
        if(audio.tone_positive != previous) transitions++;
        previous = audio.tone_positive;
    }
    CHECK(mf_passive_rf_audio_tone_complete(&audio));
    mf_passive_rf_audio_release(&audio);
    return transitions;
}

static void test_tone_and_courtesy(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;
    int16_t attack_start;
    int16_t attack_end;
    int16_t release_start = 0;
    int16_t release_end = 0;

    CHECK(tone_transitions(700U) >= 1399U && tone_transitions(700U) <= 1401U);
    CHECK(tone_transitions(850U) >= 1699U && tone_transitions(850U) <= 1701U);

    prepare_and_start(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_start_tone(&audio, 700U, 30U));
    mf_passive_rf_audio_next_pulse(&audio);
    attack_start = audio.sample;
    for(uint32_t i = 1U; i < 1440U; i++) {
        mf_passive_rf_audio_next_pulse(&audio);
        if(i == 143U) attack_end = audio.sample;
        if(i == 1296U) release_start = audio.sample;
        if(i == 1438U) release_end = audio.sample;
        CHECK(i + 1U == 1440U || !mf_passive_rf_audio_tone_complete(&audio));
    }
    CHECK(mf_passive_rf_audio_tone_complete(&audio));
    CHECK(attack_start != 0 && abs(attack_end) > abs(attack_start));
    CHECK(abs(release_start) > abs(release_end));
    mf_passive_rf_audio_release(&audio);
}

static void test_frequency_validation(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    CHECK(fake.probe_count == 6U);
    CHECK(fake.probes[0] == TEST_FREQUENCY_HZ && fake.probes[1] == TEST_FREQUENCY_HZ);
    CHECK(fake.probes[2] == TEST_FREQUENCY_HZ - MF_PASSIVE_RF_DEVIATION_HZ);
    CHECK(fake.probes[4] == TEST_FREQUENCY_HZ + MF_PASSIVE_RF_DEVIATION_HZ);

    fake.invalid_hz = TEST_FREQUENCY_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    fake.invalid_hz = TEST_FREQUENCY_HZ - MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    fake.invalid_hz = TEST_FREQUENCY_HZ + MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    fake.invalid_hz = 0U;
    fake.denied_hz = TEST_FREQUENCY_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    fake.denied_hz = TEST_FREQUENCY_HZ - MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    fake.denied_hz = TEST_FREQUENCY_HZ + MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, TEST_FREQUENCY_HZ));
    CHECK(!mf_passive_rf_audio_frequency_usable(&audio, MF_PASSIVE_RF_DEVIATION_HZ - 1U));
    CHECK(!mf_passive_rf_audio_frequency_usable(
        &audio, UINT32_MAX - MF_PASSIVE_RF_DEVIATION_HZ + 1U));
}

static void test_preset_and_lifecycle(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    CHECK(audio.prepared && !audio.running && !audio.insomnia_owned);
    CHECK(fake.preset_size == 46U);
    CHECK(preset_has(&fake, 0x02U, 0x0DU));
    CHECK(preset_has(&fake, 0x0BU, 0x06U));
    CHECK(preset_has(&fake, 0x08U, 0x32U));
    CHECK(preset_has(&fake, 0x07U, 0x04U));
    CHECK(preset_has(&fake, 0x14U, 0x00U));
    CHECK(preset_has(&fake, 0x13U, 0x02U));
    CHECK(preset_has(&fake, 0x12U, 0x04U));
    CHECK(preset_has(&fake, 0x11U, 0xE4U));
    CHECK(preset_has(&fake, 0x10U, 0x6AU));
    CHECK(preset_has(&fake, 0x15U, 0x04U));
    CHECK(preset_has(&fake, 0x18U, 0x18U));
    CHECK(preset_has(&fake, 0x19U, 0x16U));
    CHECK(preset_has(&fake, 0x1DU, 0x91U));
    CHECK(preset_has(&fake, 0x1CU, 0x00U));
    CHECK(preset_has(&fake, 0x1BU, 0x07U));
    CHECK(preset_has(&fake, 0x20U, 0xFBU));
    CHECK(preset_has(&fake, 0x22U, 0x10U));
    CHECK(preset_has(&fake, 0x21U, 0x56U));
    CHECK(fake.preset[38] == 0xC0U);

    mf_passive_rf_audio_release(&audio);
    setup(&audio, &pipe, &fake);
    fake.tuned_frequency_hz = TEST_FREQUENCY_HZ - 302U;
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    CHECK(audio.prepared && fake.last_frequency_hz == TEST_FREQUENCY_HZ);

    mf_passive_rf_audio_release(&audio);
    setup(&audio, &pipe, &fake);
    fake.tuned_frequency_hz = TEST_FREQUENCY_HZ + 95U;
    fake.denied_hz = fake.tuned_frequency_hz + MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    CHECK(!audio.prepared && fake.sleeping);

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));

    fake.call_count = 0U;
    audio.pdm_error = 1234;
    audio.source_phase = 99U;
    audio.duration_slot = 5U;
    audio.sample = 1234;
    CHECK(mf_passive_rf_audio_start_burst(&audio));
    CHECK(
        audio.pdm_error == 0 && audio.source_phase == 0U && audio.duration_slot == 0U &&
        audio.sample == 0);
    CHECK(fake.async_context == &audio);
    CHECK(fake.calls[0] == CallSetFrequency);
    CHECK(fake.calls[1] == CallInsomniaEnter);
    CHECK(fake.calls[2] == CallAsyncStart);
    CHECK(fake.calls[3] == CallLedOn);
    mf_passive_rf_audio_pause(&audio);
    CHECK(fake.calls[4] == CallAsyncStop);
    CHECK(fake.calls[5] == CallLedOff);
    CHECK(fake.calls[6] == CallIdle);
    CHECK(fake.calls[7] == CallInsomniaExit);
    CHECK(fake.insomnia_enters == 1U && fake.insomnia_exits == 1U);
    CHECK(!fake.led_on && !audio.running && !audio.insomnia_owned);
    mf_passive_rf_audio_pause(&audio);
    CHECK(fake.insomnia_exits == 1U && fake.async_stops == 1U);
    mf_passive_rf_audio_release(&audio);
    mf_passive_rf_audio_release(&audio);
    CHECK(audio.pipe == NULL && !audio.prepared && !audio.running && !fake.led_on);
    CHECK(fake.insomnia_enters == fake.insomnia_exits);
    CHECK(fake.sleeping);
}

static void test_failures_and_region_change(void) {
    MfPassiveRfAudio audio;
    MfPassivePcmPipe pipe;
    FakeHardware fake;

    setup(&audio, &pipe, &fake);
    fake.preset_ok = false;
    CHECK(!mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    CHECK(!audio.prepared && !audio.running && !fake.led_on && fake.sleeping);

    setup(&audio, &pipe, &fake);
    fake.tuned_frequency_hz = 0U;
    CHECK(!mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    CHECK(!audio.prepared && !audio.running && !fake.led_on && fake.sleeping);

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    fake.async_start_ok = false;
    CHECK(!mf_passive_rf_audio_start_burst(&audio));
    CHECK(fake.insomnia_enters == 1U && fake.insomnia_exits == 1U);
    CHECK(!audio.running && !audio.prepared && !fake.led_on && fake.sleeping);

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    fake.tuned_frequency_hz = 0U;
    CHECK(!mf_passive_rf_audio_start_burst(&audio));
    CHECK(fake.async_starts == 0U && fake.insomnia_enters == 0U);
    CHECK(!audio.running && !audio.prepared && !fake.led_on && fake.sleeping);

    setup(&audio, &pipe, &fake);
    CHECK(mf_passive_rf_audio_prepare(&audio, TEST_FREQUENCY_HZ));
    fake.denied_hz = TEST_FREQUENCY_HZ + MF_PASSIVE_RF_DEVIATION_HZ;
    CHECK(!mf_passive_rf_audio_start_burst(&audio));
    CHECK(fake.async_starts == 0U && fake.insomnia_enters == 0U);
    CHECK(!audio.prepared && !audio.running && !fake.led_on && fake.sleeping);
}

static void test_constants(void) {
    CHECK(MF_PASSIVE_PCM_RING_SAMPLES == 1024U);
    CHECK(MF_PASSIVE_VOICE_PIPE_HIGH_WATER == 960U);
    CHECK(MF_PASSIVE_VOICE_PIPE_PRIME_SAMPLES == 128U);
    CHECK(MF_PASSIVE_VOICE_READ_MAX == 512U);
}

int main(void) {
    test_duration_cycle();
    test_voice_pdm_and_gain();
    test_voice_pacing_wrap_and_eof();
    test_underrun_and_silence();
    test_tone_and_courtesy();
    test_frequency_validation();
    test_preset_and_lifecycle();
    test_failures_and_region_change();
    test_constants();
    printf("test_passive_rf_audio: %u checks passed\n", checks);
    return 0;
}
