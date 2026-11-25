/* Host tests of the signal editing: a RAW .sub loaded into the working file,
 * the RAM window, Save, Copy, Cut, Paste and Undo. Run with `make -C tests`. */

#include "../src/app.h"

#define MAX_SAMPLES 8192

static int failures;
#define CHECK(c)                                                  \
    do {                                                          \
        if(!(c)) {                                                \
            printf("%s:%d: FAILED %s\n", __FILE__, __LINE__, #c); \
            failures++;                                           \
        }                                                         \
    } while(0)

static const char* data_path = "data/random.sub";
static int16_t want[MAX_SAMPLES], got[MAX_SAMPLES];
static size_t n_want;
static int32_t t_want;

// RAW values of a .sub the way loading stores them: split into DUR_CLAMP pieces.
static size_t parse_sub(const char* path, int16_t* out) {
    static char line[16384];
    size_t n = 0;
    FILE* f = fopen(path, "r");
    while(f && fgets(line, sizeof(line), f)) {
        if(strncmp(line, "RAW_Data:", 9)) continue;

        char *p = line + 9, *end;
        for(long v = strtol(p, &end, 10); end != p; p = end, v = strtol(p, &end, 10)) {
            long sign = v < 0 ? -1 : 1, mag = labs(v);
            do {
                long c = mag > DUR_CLAMP ? DUR_CLAMP : mag;
                out[n++] = (int16_t)(sign * c);
                mag -= c;
            } while(mag > 0);
        }
    }

    if(f) fclose(f);

    return n;
}

static bool work_is(const int16_t* d, size_t n) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", host_root, WORK_PATH);
    FILE* f = fopen(path, "rb");
    size_t m = f ? fread(got, sizeof(int16_t), MAX_SAMPLES, f) : 0;
    if(f) fclose(f);

    return m == n && !memcmp(got, d, n * sizeof(int16_t));
}

// Open the test capture with RAM for a window of `window` samples.
static App* open_signal(size_t window) {
    host_heap =
        sizeof(App) + UNDO_MAX * sizeof(UndoEntry) + window * sizeof(int16_t) + LOAD_HEAP_RESERVE;
    App* a = malloc(sizeof(App));
    memset(a, 0, sizeof(App));
    WorkWriter* w = work_begin(a->st);
    bool ok = w && work_add_sub(a->st, data_path, w, &a->sd, MergeJoinNone);
    ok = w && work_end(w, &a->sd) && ok;
    CHECK(ok && work_open(a));
    a->marker_b = a->view_end = a->sd.win_us;
    clamp_view(a);
    return a;
}

static void close_signal(App* a) {
    free(a->sd.data);
    free(a->undo);
    work_cleanup(a->st);
    free(a);
}

static uint32_t rnd(uint32_t n) {
    static uint32_t s = 12345;
    s = s * 1103515245 + 12345;
    return (s >> 8) % n;
}

static void test_load_save(void) {
    App* a = open_signal(MAX_SAMPLES);
    CHECK(!work_windowed(&a->sd));
    CHECK(a->sd.file_count == n_want);
    CHECK(a->sd.total_us == t_want);
    CHECK(work_is(want, n_want));

    // The whole signal saved is the signal (it starts on a pulse and ends on a gap: no trim)
    CHECK(write_selection(a->st, a, "out/save.sub"));
    CHECK(
        parse_sub("out/save.sub", got) == n_want && !memcmp(got, want, n_want * sizeof(int16_t)));
    close_signal(a);
}

static bool window_ok(const App* a) {
    const SubData* sd = &a->sd;
    return !memcmp(sd->data, want + sd->win_s0, sd->count * sizeof(int16_t)) &&
           sd->win_t0 == samples_us(want, sd->win_s0) &&
           sd->win_us == samples_us(sd->data, sd->count);
}

static void test_window(void) {
    App* a = open_signal(400);
    CHECK(work_windowed(&a->sd) && a->sd.count == 400);
    while(work_slide(a, 1))
        CHECK(window_ok(a));

    CHECK(a->sd.win_s0 + a->sd.count == n_want);
    while(work_slide(a, -1))
        CHECK(window_ok(a));

    CHECK(a->sd.win_s0 == 0);
    close_signal(a);
}

// Copy A..B, cut it and paste it back where it was: the file must come back to the byte.
static void test_cut_paste(size_t window) {
    App* a = open_signal(window);
    for(int k = 0; k < 40; k++) {
        if(k % 4 == 0) work_slide(a, 1); // move the window now and then

        int32_t t0 = a->sd.win_t0, w = a->sd.win_us;
        a->marker_a = t0 + (int32_t)rnd((uint32_t)w / 2);
        a->marker_b = a->marker_a + 1 + (int32_t)rnd((uint32_t)w / 3);
        a->active = 0;
        int32_t len = a->marker_b - a->marker_a;

        CHECK(clip_store(a) && a->clip_us == len);
        CHECK(work_cut(a));
        CHECK(a->sd.total_us == t_want - len);
        CHECK(work_paste(a));
        CHECK(a->sd.total_us == t_want);
        CHECK(a->marker_b - a->marker_a == len); // the paste is selected
        CHECK(work_is(want, n_want));
        CHECK(window_ok(a));
    }

    CHECK(a->undo_n == UNDO_MAX);
    close_signal(a);
}

// A paste elsewhere makes the signal longer: with RAM for it the window still holds all of it.
static void test_paste_grows(void) {
    App* a = open_signal(MAX_SAMPLES);
    a->marker_a = 200000, a->marker_b = 400000;
    CHECK(clip_store(a));
    a->marker_a = 2000000;
    CHECK(work_paste(a));
    CHECK(a->sd.total_us == t_want + 200000 && a->sd.file_count > n_want);
    CHECK(!work_windowed(&a->sd) && a->marker_b == 2200000);
    close_signal(a);
}

static void test_undo(void) {
    static int16_t after_first[MAX_SAMPLES];
    App* a = open_signal(MAX_SAMPLES);

    a->marker_a = 100000, a->marker_b = 300000;
    CHECK(work_cut(a));
    size_t n_first = a->sd.file_count;
    memcpy(after_first, a->sd.data, n_first * sizeof(int16_t));

    a->marker_a = 1000000, a->marker_b = 1500000;
    CHECK(work_cut(a) && a->undo_n == 2);

    char label[32];
    undo_label(a, 1, label, sizeof(label));
    CHECK(!strncmp(label, "2. Cut -", 8));

    CHECK(work_undo(a, 1) && a->undo_n == 1 && a->sd.file_count == n_first);
    CHECK(work_is(after_first, n_first));
    CHECK(work_undo(a, 0) && a->undo_n == 0);
    CHECK(work_is(want, n_want) && a->sd.total_us == t_want);
    CHECK(a->marker_a == 100000 && a->marker_b == 300000);
    CHECK(!storage_file_exists(a->st, UNDO_DIR "/undo_0.bin"));
    CHECK(!storage_file_exists(a->st, UNDO_DIR "/undo_1.bin"));
    close_signal(a);
}

int main(void) {
    storage_simply_mkdir(NULL, "out");
    storage_simply_mkdir(NULL, "/data");
    n_want = parse_sub(data_path, want);
    t_want = samples_us(want, n_want);
    CHECK(n_want > 3000); // gaps over DUR_CLAMP took more than one sample

    test_load_save();
    test_window();
    test_cut_paste(MAX_SAMPLES);
    test_cut_paste(400);
    test_paste_grows();
    test_undo();

    printf(failures ? "%d check(s) failed\n" : "All tests passed\n", failures);
    return failures != 0;
}
