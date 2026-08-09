#include "morse_flipper_app_i.h"

#if MF_TLM

#include <cli/cli.h>
#include <stdio.h>
#include <stdlib.h>

#define MF_TLM_RING_COUNT 64U
#define MF_TLM_LINE_LEN   384U

typedef struct {
    uint32_t seq;
    char line[MF_TLM_LINE_LEN];
} MfTlmRecord;

static MfTlmRecord mf_tlm_ring[MF_TLM_RING_COUNT];
static FuriMutex* mf_tlm_mutex = NULL;
static uint32_t mf_tlm_next_seq = 1U;
static uint32_t mf_tlm_oldest_seq = 1U;
static uint8_t mf_tlm_count = 0U;
static bool mf_tlm_registered = false;

static uint8_t mf_tlm_group_index(const MorseFlipperApp* app) {
    uint8_t idx;

    if(app == NULL) return 0U;
    idx = morse_trainer_session_index(&app->trainer);
    return idx > 0U ? (uint8_t)(idx - 1U) : 0U;
}

static const char* mf_tlm_pc_mode(uint8_t mode) {
    switch(mode) {
    case MorseFlipperPcModeKeyboard:
        return "keyboard";
    case MorseFlipperPcModeMouse:
        return "mouse";
    case MorseFlipperPcModeMidi:
        return "midi";
    default:
        return "off";
    }
}

static const char* mf_tlm_input_source(uint8_t source) {
    switch(source) {
    case MorseFlipperInputSourceStraight:
        return "straight";
    case MorseFlipperInputSourcePaddle:
        return "paddle";
    default:
        return "buttons";
    }
}

static const char* mf_tlm_audio_path(uint8_t path) {
    switch(path) {
    case MorseFlipperAudioPathGpioP2Hd:
        return "pa2";
    case MorseFlipperAudioPathVibration:
        return "vibration";
    default:
        return "buzzer";
    }
}

static const char* mf_tlm_waveform(uint8_t path) {
    return path == MorseFlipperAudioPathSoftBuzz ? "sine" : "square";
}

static const char* mf_tlm_charset_source(const MorseFlipperApp* app) {
    if(app == NULL) return "lesson";
    return morse_flipper_effective_trainer_custom_set_idx(app) == 0U ? "lesson" : "custom";
}

static uint32_t mf_tlm_answer_timeout_ms(const MorseFlipperApp* app) {
    uint8_t timeout_s = app != NULL ? app->trainer_answer_timeout_s :
                                      MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    if(timeout_s == 0U) timeout_s = MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S;
    return (uint32_t)timeout_s * 1000U;
}

static void mf_tlm_escape(char* out, size_t out_sz, const char* in) {
    size_t wi = 0U;
    size_t ri = 0U;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(in == NULL) return;

    while(in[ri] != '\0' && wi + 1U < out_sz) {
        char ch = in[ri++];

        if((ch == '"' || ch == '\\') && wi + 2U < out_sz) {
            out[wi++] = '\\';
            out[wi++] = ch;
        } else if((uint8_t)ch >= 0x20U) {
            out[wi++] = ch;
        }
    }
    out[wi] = '\0';
}

static void mf_tlm_push_line(char* line) {
    uint32_t seq;
    uint32_t slot;

    if(line == NULL) return;
    if(mf_tlm_mutex != NULL) furi_mutex_acquire(mf_tlm_mutex, FuriWaitForever);

    seq = mf_tlm_next_seq++;
    slot = (seq - 1U) % MF_TLM_RING_COUNT;
    mf_tlm_ring[slot].seq = seq;
    mf_tlm_ring[slot].line[0] = '\0';
    strlcpy(mf_tlm_ring[slot].line, line, sizeof(mf_tlm_ring[slot].line));
    if(mf_tlm_count < MF_TLM_RING_COUNT) mf_tlm_count++;
    mf_tlm_oldest_seq = mf_tlm_next_seq - mf_tlm_count;

    if(mf_tlm_mutex != NULL) furi_mutex_release(mf_tlm_mutex);
}

static void mf_tlm_event(const char* body) {
    char line[MF_TLM_LINE_LEN];
    uint32_t seq;
    int n;

    if(body == NULL) return;
    if(mf_tlm_mutex != NULL) furi_mutex_acquire(mf_tlm_mutex, FuriWaitForever);
    seq = mf_tlm_next_seq;
    if(mf_tlm_mutex != NULL) furi_mutex_release(mf_tlm_mutex);

    n = snprintf(
        line,
        sizeof(line),
        "MFT {\"v\":1,\"seq\":%lu,\"t_ms\":%lu,%s}",
        (unsigned long)seq,
        (unsigned long)furi_get_tick(),
        body);
    if(n <= 0 || (size_t)n >= sizeof(line)) return;
    mf_tlm_push_line(line);
}

static void mf_tlm_dump(uint32_t since) {
    uint32_t oldest;
    uint32_t next;
    uint32_t start;
    uint32_t dropped;

    if(mf_tlm_mutex != NULL) furi_mutex_acquire(mf_tlm_mutex, FuriWaitForever);

    oldest = mf_tlm_oldest_seq;
    next = mf_tlm_next_seq;
    start = since + 1U;
    dropped = start < oldest ? oldest : 0U;
    if(start < oldest) start = oldest;

    if(dropped != 0U) {
        printf(
            "MFT {\"v\":1,\"event\":\"tlm_dump\",\"from_seq\":%lu,\"oldest_seq\":%lu,"
            "\"next_seq\":%lu,\"dropped_before_seq\":%lu}\r\n",
            (unsigned long)since,
            (unsigned long)oldest,
            (unsigned long)next,
            (unsigned long)dropped);
    } else {
        printf(
            "MFT {\"v\":1,\"event\":\"tlm_dump\",\"from_seq\":%lu,\"oldest_seq\":%lu,"
            "\"next_seq\":%lu,\"dropped_before_seq\":null}\r\n",
            (unsigned long)since,
            (unsigned long)oldest,
            (unsigned long)next);
    }
    for(uint32_t seq = start; seq < next; seq++) {
        uint32_t slot = (seq - 1U) % MF_TLM_RING_COUNT;
        if(mf_tlm_ring[slot].seq == seq) {
            printf("%s\r\n", mf_tlm_ring[slot].line);
        }
    }
    printf("MFT {\"v\":1,\"event\":\"tlm_dump_done\",\"next_seq\":%lu}\r\n", (unsigned long)next);

    if(mf_tlm_mutex != NULL) furi_mutex_release(mf_tlm_mutex);
}

static void mf_tlm_cli(PipeSide* pipe, FuriString* args, void* context) {
    uint32_t since = 0U;
    const char* text;
    char* end = NULL;

    UNUSED(pipe);
    UNUSED(context);

    text = furi_string_get_cstr(args);
    while(*text == ' ') {
        text++;
    }
    if(strncmp(text, "since", 5U) == 0 && (text[5] == '\0' || text[5] == ' ')) {
        text += 5U;
        while(*text == ' ') {
            text++;
        }
        since = (uint32_t)strtoul(text, &end, 10);
        if(end != text) {
            while(*end == ' ') {
                end++;
            }
            if(*end == '\0') {
                mf_tlm_dump(since);
                return;
            }
        }
    }

    printf("Usage: mf_tlm since <seq>\r\n");
}

static void mf_tlm_register(void) {
    CliRegistry* cli;

    if(mf_tlm_registered) return;
    cli = furi_record_open(RECORD_CLI);
    cli_registry_add_command(cli, "mf_tlm", CliCommandFlagParallelSafe, mf_tlm_cli, NULL);
    furi_record_close(RECORD_CLI);
    mf_tlm_registered = true;
}

static void mf_tlm_unregister(void) {
    CliRegistry* cli;

    if(!mf_tlm_registered) return;
    cli = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(cli, "mf_tlm");
    furi_record_close(RECORD_CLI);
    mf_tlm_registered = false;
}

void mf_tlm_init(const MorseFlipperApp* app) {
    memset(mf_tlm_ring, 0, sizeof(mf_tlm_ring));
    mf_tlm_next_seq = 1U;
    mf_tlm_oldest_seq = 1U;
    mf_tlm_count = 0U;
    if(mf_tlm_mutex == NULL) mf_tlm_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    mf_tlm_register();
    mf_tlm_event("\"event\":\"hello\",\"app\":\"morse_flipper\",\"build\":\"" APP_BUILD_COMMIT
                 "\",\"trace\":\"mft-v1\"");
    mf_tlm_cfg(app);
}

void mf_tlm_deinit(void) {
    mf_tlm_unregister();
    if(mf_tlm_mutex != NULL) {
        furi_mutex_free(mf_tlm_mutex);
        mf_tlm_mutex = NULL;
    }
}

void mf_tlm_cfg(const MorseFlipperApp* app) {
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"config_state\",\"usb_mode\":\"%s\",\"input_source\":\"%s\","
        "\"keyer_mode\":\"%s\",\"audio_path\":\"%s\",\"waveform\":\"%s\","
        "\"pwm_volume\":%u,\"lesson\":%u,\"group_size\":%u,\"session_groups\":%u,"
        "\"wpm\":%u,\"farnsworth_wpm\":%u",
        mf_tlm_pc_mode(app->pc_mode_pref),
        mf_tlm_input_source(app->input_source),
        morse_keyer_mode_name(app->keyer_mode),
        mf_tlm_audio_path(app->audio_path),
        mf_tlm_waveform(app->audio_path),
        (unsigned)morse_flipper_p2_volume_pct(app),
        (unsigned)morse_trainer_lesson(&app->trainer),
        (unsigned)morse_trainer_group_size(&app->trainer),
        (unsigned)morse_trainer_session_groups(&app->trainer),
        (unsigned)morse_flipper_local_wpm(app),
        (unsigned)app->trainer_farnsworth_wpm);
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_session(const MorseFlipperApp* app) {
    char charset[MORSE_TRAINER_CHARSET_CAP * 2U];
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    mf_tlm_escape(charset, sizeof(charset), morse_trainer_charset(&app->trainer));
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"session_start\",\"mode\":\"listening\",\"rng_state\":%lu,"
        "\"lesson\":%u,\"charset\":\"%s\",\"charset_source\":\"%s\","
        "\"group_size\":%u,\"session_groups\":%u,\"wpm\":%u,"
        "\"farnsworth_wpm\":%u,\"answer_timeout_ms\":%lu",
        (unsigned long)app->trainer.rng_state,
        (unsigned)morse_trainer_lesson(&app->trainer),
        charset,
        mf_tlm_charset_source(app),
        (unsigned)morse_trainer_group_size(&app->trainer),
        (unsigned)morse_trainer_session_groups(&app->trainer),
        (unsigned)morse_flipper_local_wpm(app),
        (unsigned)app->trainer_farnsworth_wpm,
        (unsigned long)mf_tlm_answer_timeout_ms(app));
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_group(const MorseFlipperApp* app) {
    char group[MORSE_TRAINER_GROUP_CAP * 2U];
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    mf_tlm_escape(group, sizeof(group), morse_trainer_last_group(&app->trainer));
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"group_ready\",\"index\":%u,\"rng_state\":%lu,\"expected\":\"%s\"",
        (unsigned)mf_tlm_group_index(app),
        (unsigned long)app->trainer.rng_state,
        group);
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_open(const MorseFlipperApp* app, uint32_t deadline_ms) {
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"answer_open\",\"index\":%u,\"deadline_ms\":%lu,"
        "\"answer_timeout_ms\":%lu",
        (unsigned)mf_tlm_group_index(app),
        (unsigned long)deadline_ms,
        (unsigned long)mf_tlm_answer_timeout_ms(app));
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_answer(const MorseFlipperApp* app, const char* actual, bool ok) {
    char group[MORSE_TRAINER_GROUP_CAP * 2U];
    char answer[MORSE_TRAINER_GROUP_CAP * 2U];
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    mf_tlm_escape(group, sizeof(group), morse_trainer_last_group(&app->trainer));
    mf_tlm_escape(answer, sizeof(answer), actual);
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"answer_final\",\"index\":%u,\"expected\":\"%s\","
        "\"actual\":\"%s\",\"ok\":%s,\"score\":%d",
        (unsigned)mf_tlm_group_index(app),
        group,
        answer,
        ok ? "true" : "false",
        (int)morse_trainer_last_score(&app->trainer));
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_done(const MorseFlipperApp* app) {
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"session_done\",\"mode\":\"listening\",\"percent\":%u,"
        "\"groups\":%u,\"failed\":%u,\"aborted\":%s",
        (unsigned)morse_trainer_session_letter_percent(&app->trainer),
        (unsigned)morse_trainer_session_total(&app->trainer),
        (unsigned)morse_trainer_session_fail_count(&app->trainer),
        morse_trainer_session_aborted(&app->trainer) ? "true" : "false");
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

#endif
