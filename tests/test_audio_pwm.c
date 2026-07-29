#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "morse_flipper_audio_pwm.h"
#include "mf_passive_audio_pipe.h"

static unsigned checks;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

static void prepare(MorseFlipperAudioPwm* audio, uint32_t sample_rate_hz) {
    morse_flipper_audio_pwm_prepare_target(
        audio, MorseFlipperAudioPwmTargetP2, 256000U, sample_rate_hz, 700U, 100U, 0U, 0U);
}

static void test_voice_rate(uint32_t output_rate_hz, uint32_t source_rate_hz) {
    MorseFlipperAudioPwm audio;
    MfPassivePcmPipe pipe = {0};
    uint16_t output[16];
    prepare(&audio, output_rate_hz);
    pipe.samples[0] = -32768;
    pipe.samples[1] = 0;
    pipe.samples[2] = 32767;
    pipe.samples[3] = 0;
    pipe.write_pos = 4U;
    morse_flipper_audio_pwm_set_voice(&audio, &pipe, source_rate_hz);
    CHECK(audio.voice_primed);
    CHECK(pipe.read_pos == 2U);
    morse_flipper_audio_pwm_render(&audio, output, sizeof(output) / sizeof(output[0]));
    CHECK(output[0] != output[1]);
    CHECK(pipe.read_pos > 2U && pipe.read_pos <= pipe.write_pos);
    pipe.read_pos = pipe.write_pos;
    pipe.eof = true;
    morse_flipper_audio_pwm_render(&audio, output, sizeof(output) / sizeof(output[0]));
    CHECK(pipe.drained);
    CHECK(audio.source == MorseFlipperAudioPwmSourceSilence);
    CHECK(output[sizeof(output) / sizeof(output[0]) - 1U] == audio.pwm_midpoint);
}

static void test_wrap_and_underrun(void) {
    MorseFlipperAudioPwm audio;
    MfPassivePcmPipe pipe = {
        .read_pos = MF_PASSIVE_PCM_RING_SAMPLES - 2U,
        .write_pos = 2U,
    };
    uint16_t output[20];
    prepare(&audio, 32000U);
    pipe.samples[MF_PASSIVE_PCM_RING_SAMPLES - 2U] = -100;
    pipe.samples[MF_PASSIVE_PCM_RING_SAMPLES - 1U] = 100;
    pipe.samples[0] = 200;
    pipe.samples[1] = 300;
    morse_flipper_audio_pwm_set_voice(&audio, &pipe, 8000U);
    morse_flipper_audio_pwm_render(&audio, output, sizeof(output) / sizeof(output[0]));
    CHECK(pipe.read_pos < MF_PASSIVE_PCM_RING_SAMPLES);
    pipe.read_pos = pipe.write_pos;
    pipe.eof = false;
    morse_flipper_audio_pwm_render(&audio, output, sizeof(output) / sizeof(output[0]));
    CHECK(pipe.underruns != 0U);
    morse_flipper_audio_pwm_set_silence(&audio);
    CHECK(audio.voice_pipe != NULL);
    CHECK(audio.source == MorseFlipperAudioPwmSourceSilence);
}

static void test_eof_holds_final_sample(void) {
    MorseFlipperAudioPwm audio;
    MfPassivePcmPipe pipe = {0};
    uint16_t output[12];
    prepare(&audio, 32000U);
    pipe.samples[0] = -32768;
    pipe.samples[1] = 32767;
    pipe.write_pos = 2U;
    pipe.eof = true;
    morse_flipper_audio_pwm_set_voice(&audio, &pipe, 8000U);
    CHECK(audio.source == MorseFlipperAudioPwmSourceVoice);
    morse_flipper_audio_pwm_render(&audio, output, 4U);
    CHECK(!pipe.drained && audio.source == MorseFlipperAudioPwmSourceVoiceTail);
    CHECK(output[2] <= audio.pwm_period);
    morse_flipper_audio_pwm_render(&audio, output + 4U, 4U);
    CHECK(pipe.drained && audio.source == MorseFlipperAudioPwmSourceSilence);
    CHECK(output[4] == output[5] && output[5] == output[6]);
    morse_flipper_audio_pwm_render(&audio, output + 8U, 4U);
    CHECK(output[8] == audio.pwm_midpoint);

    prepare(&audio, 32000U);
    memset(&pipe, 0, sizeof(pipe));
    pipe.samples[0] = 1234;
    pipe.write_pos = 1U;
    pipe.eof = true;
    morse_flipper_audio_pwm_set_voice(&audio, &pipe, 8000U);
    CHECK(audio.source == MorseFlipperAudioPwmSourceVoiceTail);
    morse_flipper_audio_pwm_render(&audio, output, 4U);
    CHECK(pipe.drained);
}

int main(void) {
    test_voice_rate(32000U, 8000U);
    test_voice_rate(32000U, 16000U);
    test_voice_rate(31250U, 8000U);
    test_voice_rate(31250U, 16000U);
    test_wrap_and_underrun();
    test_eof_holds_final_sample();
    printf("test_audio_pwm: %u checks passed\n", checks);
    return 0;
}
