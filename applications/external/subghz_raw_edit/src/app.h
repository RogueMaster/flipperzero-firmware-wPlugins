#pragma once

/*
 * Sub-GHz RAW Edit - a tiny waveform editor for Flipper Zero RAW .sub captures.
 * https://github.com/Lechnio/SubGHz-RAW-Edit
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>

#include <lib/toolbox/level_duration.h>
#include <lib/toolbox/stream/stream.h>
#include <lib/toolbox/stream/file_stream.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_protocol_registry.h>
#ifndef FW_ORIGIN_Official
#include <lib/subghz/blocks/custom_btn.h>
#include <furi_hal_subghz.h>
#else
// Official firmware has no custom buttons: always send the recorded button.
#define SUBGHZ_CUSTOM_BTN_OK 0
#endif

#include "subghz_raw_edit_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBGHZ_DIR          "/ext/subghz"
#define SYNTH_YIELD_CAP     4096
#define NAME_SCROLL_HOLD_MS 1500
#define NAME_SCROLL_STEP_MS 300
#define DUR_CLAMP           32000
#define LOAD_HEAP_RESERVE   16384 // Decode (DECODE_HEAP_RESERVE + its list), Save and dialogs
#define LINE_RESERVE        4096 // Sub-GHz writes up to 512 RAW values (~3.5 KB) per line

#define GAP_KEEP_MAX_US 1500

#define MERGE_GAP_DEFAULT_MS 100
#define MERGE_GAP_MIN_MS     0
// A single sample is int16_t clamped to DUR_CLAMP (32000 us = 32 ms), so gaps
// longer than that are emitted as several consecutive silence samples.
#define MERGE_GAP_MAX_MS     1000
#define MERGE_REPEAT_DEFAULT 1
#define MERGE_REPEAT_MIN     1
#define MERGE_REPEAT_MAX     64
#define STR_(x)              #x
#define STR(x)               STR_(x)
#define MERGE_PATH_LEN       128
#define MERGE_BUF_CHUNK      512

#define APP_VERSION "1.10"
#define APP_REPO    "github.com/Lechnio/SubGHz-RAW-Edit"
#define APP_NAME    "Sub-GHz RAW Edit"

// Custom buttons and keeping the rolling counter need a custom firmware's SDK
#ifdef FW_ORIGIN_Official
#define APP_CUSTOM_FW_FEATURES false
#else
#define APP_CUSTOM_FW_FEATURES true
#endif

#define SCREEN_W_PX  128
#define SCREEN_H_PX  64
#define FONT_SIZE_PX 7
#define GAP_PX       1

#define TOP_TEXT_BASE FONT_SIZE_PX
#define OV_Y          (TOP_TEXT_BASE + GAP_PX)
#define OV_H          5
#define WAVE_H        32
#define WAVE_TOP      (OV_Y + OV_H + 1)
#define WAVE_BOT      (WAVE_TOP + WAVE_H)
#define SEP_Y         (WAVE_BOT + 1)
#define LINE1_BASE    (SEP_Y + 1 + GAP_PX + FONT_SIZE_PX)
#define LINE2_BASE    (LINE1_BASE + 1 + FONT_SIZE_PX)

static_assert(LINE2_BASE <= SCREEN_H_PX);

#define MENU_SEL_SAVE   0
#define MENU_SEL_DECODE 1
#define MENU_SEL_CUT    2
#define MENU_SEL_UNDO   3
#define MENU_ITEMS      4

#define DECODE_MAX          32
#define DECODE_TAIL_US      100000
#define DECODE_LEAD_MIN_US  2000
#define DECODE_LEAD_MAX_US  DUR_CLAMP
#define DECODE_LEAD_US      18500
#define DECODE_HEAP_RESERVE 10240 // contiguous: the biggest decoder (BinRAW) takes ~8.8 KB
#define DECODE_KEYS_HEAP    32768

typedef struct {
    int16_t* data;
    size_t count;
    size_t cap;
    size_t limit; // sample_budget() when loading started
    int32_t total_us;
    uint32_t frequency;
    char preset[48];
    bool truncated;
    bool out_of_memory;
    bool synthesized;
} SubData;

typedef enum {
    EditModeEdit = 0,
    EditModeMenu = 1,
} EditMode;

typedef struct {
    SubData sd;
    char basename[48];
    bool src_in_dir; // the open file is SUBGHZ_DIR/<basename>.sub
    uint32_t opened_tick; // a name too long for the top bar scrolls from here

    int32_t view_start;
    int32_t view_end;
    int32_t marker_a;
    int32_t marker_b;
    int active;

    uint16_t activity[SCREEN_W_PX];
    uint16_t act_max;
    uint16_t overview[SCREEN_W_PX];
    uint16_t ov_max;
    bool wave_mode;

    char status[80];
    uint32_t status_until;

    const char* busy; // full-screen message while loading, decoding or saving

    int mode;
    int menu_sel;

    int16_t* undo_data;
    size_t undo_count;
    int32_t undo_total_us;
    int32_t undo_marker_a;
    int32_t undo_marker_b;
    int undo_active;
    int32_t undo_view_start;
    int32_t undo_view_end;
    bool has_undo;

    FuriMutex* mutex;
} App;

static inline int32_t iabs32(int32_t v) {
    return v < 0 ? -v : v;
}

static inline int32_t clamp_dur(int32_t v) {
    if(v > DUR_CLAMP) return DUR_CLAMP;

    if(v < -DUR_CLAMP) return -DUR_CLAMP;

    return v;
}

typedef struct {
    File* file;
    uint8_t buf[128];
    size_t len;
    size_t pos;
    bool eof;
} LineReader;

#ifndef FW_ORIGIN_Official
#define BTN_SLOTS     5
#define BTN_LABEL_LEN 20

typedef struct {
    bool available;
    char protocol[32];
    char label[BTN_SLOTS][BTN_LABEL_LEN];
} BtnPick;
#endif

// How a signal being appended is joined to what's already in the buffer.
typedef enum {
    MergeJoinNone, // first signal of the whole merge; nothing precedes it
    MergeJoinManual, // start of a new file: separate with the manual gap
    MergeJoinNative, // a repetition of the same signal: keep its native gap
} MergeJoin;

typedef struct {
    const SubGhzProtocol* proto;
    uint32_t hash;
    uint16_t frames;
    char key[18];
    uint32_t first, last; // samples of the entry's first frame: its first and last pulse
} Decoded;

typedef struct {
    SubGhzEnvironment* env;
    size_t i0, i1;
    uint32_t lead_us;
    Decoded* list;
    size_t n;
    const Decoded* want; // replay mode: stop at the first frame matching this entry
    FuriString* text;
    const int16_t* data;
    size_t run, run_end; // the run being fed: samples [run, run_end)
    uint32_t run_gap; // its length when it is a gap, else 0
    bool hit;
    bool oom;
} DecodeCtx;

typedef struct {
    ViewDispatcher* vd;
    bool confirmed;
    uint32_t index;
} PromptCtx;

/* ---- Shared between modules ---- */

/* The SDK builds apps with -mlong-calls so they can reach the firmware, but
 * calls between these modules stay inside the app: plain BL calls are enough
 * and save the literal and register a long call costs at every call site. */
#pragma no_long_calls

// view.c
extern const char busy_loading[];
void clamp_marker(App* a, int idx);
void clamp_view(App* a);
void ensure_visible(App* a, int32_t m);
void recompute_activity(App* a);
void recompute_overview(App* a);
void select_frame(App* a, int32_t start, int32_t end, int32_t gap);
void auto_detect(App* a);
void draw_busy(Canvas* c, const char* text, const char* name);
void draw_cb(Canvas* c, void* ctx);
void input_cb(InputEvent* e, void* ctx);
void zoom(App* a, bool in);

// load.c
extern int32_t g_merge_repeat;
extern int32_t g_merge_gap_ms;
extern bool g_normalize_jitter;
extern uint8_t g_custom_btn_id;
bool lr_read_line(LineReader* lr, FuriString* out);
void* safe_malloc(size_t size);
void* safe_realloc(void* ptr, size_t size);
size_t sample_budget(void);
SubGhzEnvironment* subghz_env_setup(void);
bool push_duration(SubData* sd, int32_t v);
size_t duration_samples(int32_t mag);
#ifndef FW_ORIGIN_Official
void btn_probe(Storage* storage, const char* path, const char* protocol, BtnPick* out);
#endif
void normalize_jitter(SubData* sd);
void recompute_total_us(SubData* sd);
bool parse_sub_meta(
    const char* s,
    uint32_t* freq,
    char* preset,
    size_t preset_sz,
    char* protocol,
    size_t proto_sz);
bool load_sub(Storage* storage, const char* path, SubData* sd);
size_t count_sub_samples(
    Storage* storage,
    const char* path,
    uint32_t* freq,
    char* preset,
    size_t preset_sz);
bool load_signal(Storage* storage, const char* path, SubData* out);
void append_signal(SubData* dst, const SubData* sig, MergeJoin join);
bool load_sub_repeated(Storage* storage, const char* path, SubData* out);

// edit_ops.c
void propose_name(
    Storage* st,
    const char* base,
    const char* sep,
    bool bare,
    char* out,
    size_t outlen);
void propose_edit_name(Storage* st, const App* a, char* out, size_t outlen);
bool selection_range(const App* a, size_t* i0, size_t* i1);
bool write_selection(Storage* st, App* a, const char* path);
bool snapshot_take(App* a);
void snapshot_restore(App* a);
size_t cut_run(App* a, bool apply);
void cut_compact(App* a);

// decode.c
bool write_decoded(Storage* st, App* a, DecodeCtx* dc, const Decoded* d, const char* path);
void decoded_base_name(const DecodeCtx* dc, const Decoded* d, char* out, size_t outlen);
void do_decode_action(Gui* gui, ViewPort* vp, FuriMessageQueue* queue, Storage* st, App* a);

// ui.c
bool btn_select_for_file(Gui* gui, Storage* storage, const char* path);
void pick_index_cb(void* ctx, uint32_t index);
void button_ok_cb(GuiButtonType button, InputType type, void* ctx);
bool prompt_run(Gui* gui, PromptCtx* pc, View* view);
bool prompt_filename(Gui* gui, char* namebuf, size_t buflen);
bool confirm(Gui* gui, const char* question);
void show_message(DialogsApp* dialogs, const char* header, const char* text, uint8_t text_y);

// editor.c
void prompt_and_save(
    Gui* gui,
    ViewPort* vp,
    FuriMessageQueue* queue,
    Storage* st,
    App* a,
    DecodeCtx* dc,
    const Decoded* d);
void run_editor_session(
    Gui* gui,
    ViewPort* vp,
    FuriMessageQueue* queue,
    Storage* storage,
    DialogsApp* dialogs,
    App* app,
    bool auto_select);
void run_editor(Storage* storage, DialogsApp* dialogs, const char* file);

// merge.c
void run_merge(Storage* storage, DialogsApp* dialogs);

#pragma long_calls_off
