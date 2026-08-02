#include "morse_flipper_app_i.h"

#if MF_TLM

#include <cli/cli.h>
#include <stdio.h>
#include <stdlib.h>

#define MF_TLM_RING_COUNT 4U
#define MF_TLM_LINE_LEN   160U

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

    printf(
        "MFT {\"v\":1,\"event\":\"tlm_dump\",\"next_seq\":%lu,\"dropped_before_seq\":%lu}\r\n",
        (unsigned long)next,
        (unsigned long)dropped);
    for(uint32_t seq = start; seq < next; seq++) {
        uint32_t slot = (seq - 1U) % MF_TLM_RING_COUNT;
        if(mf_tlm_ring[slot].seq == seq) {
            printf("%s\r\n", mf_tlm_ring[slot].line);
        }
    }

    if(mf_tlm_mutex != NULL) furi_mutex_release(mf_tlm_mutex);
}

static void mf_tlm_cli(PipeSide* pipe, FuriString* args, void* context) {
    uint32_t since = 0U;
    const char* text;

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
        since = (uint32_t)strtoul(text, NULL, 10);
        mf_tlm_dump(since);
        return;
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
        "\"event\":\"config_state\",\"usb_mode\":\"%s\"",
        mf_tlm_pc_mode(app->pc_mode_pref));
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_session(const MorseFlipperApp* app) {
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    n = snprintf(body, sizeof(body), "\"event\":\"session_start\",\"mode\":\"listening\"");
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
        "\"event\":\"group_ready\",\"index\":%u,\"expected\":\"%s\"",
        (unsigned)mf_tlm_group_index(app),
        group);
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_open(const MorseFlipperApp* app, uint32_t deadline_ms) {
    char body[MF_TLM_LINE_LEN];
    int n;

    UNUSED(deadline_ms);
    if(app == NULL) return;
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"answer_open\",\"index\":%u",
        (unsigned)mf_tlm_group_index(app));
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_answer(const MorseFlipperApp* app, const char* actual, bool ok) {
    char body[MF_TLM_LINE_LEN];
    int n;

    UNUSED(actual);
    if(app == NULL) return;
    n = snprintf(
        body,
        sizeof(body),
        "\"event\":\"answer_final\",\"index\":%u,\"ok\":%s",
        (unsigned)mf_tlm_group_index(app),
        ok ? "true" : "false");
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

void mf_tlm_done(const MorseFlipperApp* app) {
    char body[MF_TLM_LINE_LEN];
    int n;

    if(app == NULL) return;
    n = snprintf(body, sizeof(body), "\"event\":\"session_done\",\"mode\":\"listening\"");
    if(n <= 0 || (size_t)n >= sizeof(body)) return;
    mf_tlm_event(body);
}

#endif
