/*
 * blackbox.cpp — black-box incident recorder (ESP32, #124). See blackbox.h.
 *
 * Layout: a common section (ring buffer, PSRAM detect, trigger/flush logic,
 * .json build) and a storage section split by a compile-time backend, chosen
 * per board exactly like can_dump.cpp's per-board #if blocks:
 *   - LittleFS  : boards with a real data partition (waveshare-s3, lilygo-t2can)
 *   - SD        : LILYGO (reuses the SD volume can_dump.cpp already mounts)
 *   - RAM       : min_spiffs boards — keep the last event in RAM, download-only
 */

#include "blackbox.h"
#include "config.h"
#include "../../fsd_logic/fsd_capture.h"           // tesla_format_candump_line
#include "../../fsd_logic/fsd_events.h"            // fsd_events_inject
#include "../../fsd_logic/fsd_blackbox_summary.h"  // fsd_blackbox_format_json
#include "../../fsd_logic/fsd_blackbox_filter.h"   // fsd_blackbox_should_record
#include "blackbox_flush.h"                         // ring push, window search, slices
#include "blackbox_name.h"                          // event names, oldest first
#include <stdlib.h>
#include <string.h>

// Backend selection (BLACKBOX_BACKEND_*) + the enable-default live in blackbox.h
// so main.cpp / prefs.cpp share one source of truth.

// ── Tunable window ───────────────────────────────────────────────────────────
#ifndef BLACKBOX_PRE_MS
#define BLACKBOX_PRE_MS   10000u   // pre-roll kept before the trigger
#endif
#ifndef BLACKBOX_POST_MS
#define BLACKBOX_POST_MS   5000u   // post-roll recorded after the trigger
#endif
// Ring capacity in frames @ 19 B/frame (BBFrame carries a 1-byte RX/TX tag).
// PSRAM (runtime-detected) holds a full ~15 s window at a busy bus rate. The
// internal-RAM fallback must cover the window on its own — PSRAM is never
// required, and the two lilygo-t2can builds are the only ones that enable it:
//   - S3-class (512 KB SRAM): 6000 frames ≈ 114 KB → ≥15 s even at ~400 f/s.
//     Only paired with the persistent disk backends here, which stream the ring
//     straight to file (no frozen copy), so the ring is the whole footprint.
//   - Classic ESP32: 3000 frames ≈ 57 KB safety cap. These are the volatile
//     RAM-backend boards (default OFF); a flush copies the in-window frames once
//     into the kept-event slot (blackbox_ram.h frees the previous event first),
//     so the transient peak is one window, not two (#191), bounded by this cap.
// All allocation/heap-guard math uses sizeof(BBFrame), so the byte figures track
// the struct automatically — only these comments need the 18→19 B update.
#ifndef BLACKBOX_FRAMES_PSRAM
#define BLACKBOX_FRAMES_PSRAM    40000u   // ~760 KB
#endif
#ifndef BLACKBOX_FRAMES_INTERNAL
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define BLACKBOX_FRAMES_INTERNAL  6000u   // ~114 KB (S3, 512 KB SRAM)
#else
#define BLACKBOX_FRAMES_INTERNAL  3000u   // ~57 KB  (classic ESP32)
#endif
#endif
// Floor for the adaptive internal-RAM ring (#191). On a low-heap classic board
// the ring is sized down to whatever leaves the heap margin free, but never
// below this: ~800 frames ≈ 15 KB ≈ a ~4 s window — short, yet still a usable
// black box. Below even this (margin + floor can't be had) enable is refused.
#ifndef BB_MIN_INTERNAL_FRAMES
#define BB_MIN_INTERNAL_FRAMES   800u   // ~15 KB  (~4 s window)
#endif

#define BLACKBOX_DIR        "/blackbox"
#define BLACKBOX_RETAIN      5u      // keep newest N events (disk backends)
#define BLACKBOX_LINE_MAX   48u      // bytes, upper bound of one .log line
// Saving an event to disk runs a step per loop pass, this far apart: one file
// operation or one slice of .log lines (~1.2 KB). The loop, CAN included, runs
// in between. On LittleFS every 4 KB written also erases a flash sector (tens
// of ms with the cache, and so the CAN interrupt, off); written back to back
// that is most of the time, with the gap under a third of it, at ~24 KB/s.
// A slice also ends once BLACKBOX_FLUSH_BUDGET_US has gone, and the gap after
// it shrinks to match (bb_slice_gap_ms()), so the save keeps its pace. The
// MCP2515 holds two received frames. On ukinora's bench (fork commit fc7da9cb:
// ~2000 frames/s into the T-2CAN's MCP2515, a slice every loop pass) a 32-line
// slice took ~10 ms and the board kept about two frames a slice; ending slices
// at 600 us doubled what it received during a save. A flash erase inside a
// slice still runs to the end.
#define BLACKBOX_FLUSH_SLICE  32u    // .log lines per step, at most
#define BLACKBOX_FLUSH_GAP_MS 50u    // after a full slice or a file operation
#ifndef BLACKBOX_FLUSH_BUDGET_US
#define BLACKBOX_FLUSH_BUDGET_US 600u
#endif
#define BLACKBOX_TL_MAX      16      // das_ap_state timeline depth
#define BLACKBOX_TL_EMIT     12      // timeline entries written to the summary

struct __attribute__((packed)) BBFrame {
    uint32_t ts_ms;
    uint32_t id;
    uint8_t  bus;
    uint8_t  dlc;
    uint8_t  is_tx;    // 0 = RX (bus frame), 1 = TX (our injected/echoed frame)
    uint8_t  data[8];
};

// ── Common module state ──────────────────────────────────────────────────────
static FSDState*     g_state = nullptr;
static portMUX_TYPE* g_mux   = nullptr;

static BBFrame*  g_ring = nullptr;
static uint32_t  g_cap  = 0;        // ring capacity (frames); 0 = unavailable
static uint32_t  g_head = 0;        // next write slot
static uint32_t  g_tail = 0;        // oldest frame
static bool      g_ring_psram = false;
static bool      g_want_psram = false;  // size decision from init; alloc is lazy
static FSDBlackboxThin g_thin[CAN_BUS_COUNT];  // 0x257 thinning, internal-RAM ring

static bool      g_armed = false;
static BBTrigger g_trig = BB_TRIG_ABORT;
static uint32_t  g_trig_ms = 0;
static uint32_t  g_flush_at_ms = 0;
static FSDState  g_snap;            // FSDState at trigger time (toggles/hw/evt_last_*)
// A trigger that came while the armed event was being saved; armed when that
// save is over (blackbox_flush.h).
static bool      g_queued = false;
static BBTrigger g_q_trig = BB_TRIG_ABORT;
static uint32_t  g_q_ms = 0;
static FSDState  g_q_snap;
static uint32_t  g_missed = 0;      // triggers neither saved nor inside a saved window
static uint32_t  g_captures = 0;    // monotonic count since boot (badge source)
static bool      g_save_lowmem = false;  // last armed save dropped for lack of RAM (#191)
static uint32_t  g_next_seq = 1;    // number of the next event (blackbox_name.h)

// das_ap_state mini-timeline
static uint32_t  g_tl_ts[BLACKBOX_TL_MAX];
static uint8_t   g_tl_state[BLACKBOX_TL_MAX];
static uint32_t  g_tl_head = 0;
static uint32_t  g_tl_count = 0;
static uint8_t   g_tl_last = 0xFF;

static void bb_enter() { if (g_mux) portENTER_CRITICAL(g_mux); }
static void bb_exit()  { if (g_mux) portEXIT_CRITICAL(g_mux); }

static const char* trig_name(BBTrigger t) {
    switch (t) {
        case BB_TRIG_ABORT:  return "abort";
        case BB_TRIG_BUSOFF: return "busoff";
        case BB_TRIG_MANUAL: return "manual";
        default:             return "evt";
    }
}
static const char* trig_name_uc(BBTrigger t) {
    switch (t) {
        case BB_TRIG_ABORT:  return "ABORT";
        case BB_TRIG_BUSOFF: return "BUSOFF";
        case BB_TRIG_MANUAL: return "MANUAL";
        default:             return "EVT";
    }
}

static inline uint32_t ring_next(uint32_t i) { return (i + 1u) % g_cap; }

// candump interface field for a stored frame. RX keeps the plain can0/can1 so
// existing parsers (CRC cracker, candump tools) read the .log unchanged; TX (our
// injected frames) get a can0TX/can1TX suffix — a greppable, ID-preserving
// direction tag. candump has no standard direction column, so the interface
// field carries it; the ID/data fields stay in their usual positions.
static inline const char* bb_iface_name(const BBFrame& f) {
    if (f.is_tx) return f.bus == CAN_BUS_SECONDARY ? "can1TX" : "can0TX";
    return f.bus == CAN_BUS_SECONDARY ? "can1" : "can0";
}

// Event basenames are device-generated as [A-Za-z0-9_]; reject anything else so
// a crafted ?name= can't escape BLACKBOX_DIR (no '/', '.', etc.).
static bool bb_name_ok(const char* name) {
    if (!name || !name[0]) return false;
    size_t n = strlen(name);
    if (n >= 40) return false;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Storage backends
// ─────────────────────────────────────────────────────────────────────────────
#if defined(BLACKBOX_BACKEND_LITTLEFS) || defined(BLACKBOX_BACKEND_SD)

#if defined(BLACKBOX_BACKEND_LITTLEFS)
  #include <LittleFS.h>
  #define BB_FS        LittleFS
  #define BB_OPEN_W(p) BB_FS.open((p), "w")
  #define BB_OPEN_R(p) BB_FS.open((p), "r")
#else
  #include <SD.h>
  #define BB_FS        SD
  #define BB_OPEN_W(p) BB_FS.open((p), FILE_WRITE)
  #define BB_OPEN_R(p) BB_FS.open((p), FILE_READ)
#endif

static bool g_fs_ok = false;
// Cached count of saved .json events. The status/aux poll (~every 2.5 s) reads
// this instead of scanning the directory — a live scan on that hot path was
// stalling the web task and dropping WiFi (#124). Kept current by recomputing
// only inside the save/delete paths, which already touch the filesystem.
static int  g_event_count = 0;

// Strip a directory prefix and a trailing extension → bare event basename.
static void bb_basename(const char* path, char* out, size_t n) {
    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < n - 1; i++) out[i] = base[i];
    out[i] = '\0';
}

// Delete the oldest event (bb_event_order()) when more than `keep` are saved.
// True only when its .json, which is what counts as an event, was removed,
// so a caller looping on this stops on a delete the filesystem refuses.
// Not inlined: ci_check_stack.py names it on the save chain.
static __attribute__((noinline)) bool bb_drop_oldest(uint32_t keep) {
    File dir = BB_FS.open(BLACKBOX_DIR);
    if (!dir) return false;
    uint32_t count = 0;
    uint64_t oldest = UINT64_MAX;
    char oldest_base[40] = {};
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        const char* nm = e.name();
        if (strstr(nm, ".json")) {
            char base[40];
            bb_basename(nm, base, sizeof(base));
            uint64_t o = bb_event_order(base);
            count++;
            if (o < oldest) { oldest = o; strncpy(oldest_base, base, sizeof(oldest_base) - 1); }
        }
        e.close();
    }
    dir.close();
    if (count <= keep || oldest_base[0] == '\0') return false;
    char p[64];
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", oldest_base);  BB_FS.remove(p);
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", oldest_base);
    return BB_FS.remove(p);
}

// Keep only the newest BLACKBOX_RETAIN events (oldest deleted first).
static void bb_enforce_retention() {
    while (bb_drop_oldest(BLACKBOX_RETAIN)) {}
}

#if defined(BLACKBOX_BACKEND_LITTLEFS)
// Left free after every save. Retention counts events, not bytes: five full
// windows from the 40000-frame PSRAM ring (~1.7 MB of .log each) do not fit
// the 3.4 MB partition, and a LittleFS volume with no free block can fail
// even the delete that would make room.
#define BLACKBOX_FS_RESERVE  (64u * 1024u)

static size_t bb_fs_free() {
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();  // walks the filesystem: save path only
    return used < total ? total - used : 0;
}
#endif

// The save in progress (do_flush()), one step per loop pass.
enum BBSaveStep : uint8_t { BB_SAVE_IDLE = 0, BB_SAVE_ROOM, BB_SAVE_LOG, BB_SAVE_JSON, BB_SAVE_TIDY };
static struct {
    BBSaveStep step;
    uint32_t next;          // slot of the next frame to write
    uint32_t left;          // frames still to write
    uint32_t count, bus0, bus1, txc;
    uint32_t window_start;  // timestamp of the window's first frame
    uint32_t t0;            // when the save started
    char     base[40];
    File     log;
} g_save;

// The slot bb_record() must not evict: the next frame the save has to write.
static uint32_t bb_keep_slot() { return g_save.left ? g_save.next : g_cap; }

// Stop a save. Its .log goes too unless the .json already made it an event.
static void bb_save_abort() {
    if (g_save.log) g_save.log.close();
    if (g_save.step == BB_SAVE_LOG || g_save.step == BB_SAVE_JSON) {
        char p[64];
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", g_save.base);
        BB_FS.remove(p);
    }
    g_save.step = BB_SAVE_IDLE;
    g_save.left = 0;
}

// Live directory scan → number of .json events. Only ever called from the
// save/delete paths (which already touch the FS) and once at init; never from
// the status/aux poll — that reads the g_event_count cache.
static int bb_scan_count() {
    if (!g_fs_ok) return 0;
    File dir = BB_FS.open(BLACKBOX_DIR);
    if (!dir) return 0;
    int n = 0;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        if (strstr(e.name(), ".json")) n++;
        e.close();
    }
    dir.close();
    return n;
}

// A save cut short by a reset leaves a .log without its .json. Nothing counts
// that as an event, so neither retention nor the space check would ever remove
// it: do it at boot. Not inlined, for the boot chain in ci_check_stack.py.
static __attribute__((noinline)) void bb_drop_orphans() {
    for (;;) {
        File dir = BB_FS.open(BLACKBOX_DIR);
        if (!dir) return;
        char orphan[40] = {};
        for (File e = dir.openNextFile(); e && !orphan[0]; e = dir.openNextFile()) {
            if (strstr(e.name(), ".log")) {
                char base[40], p[64];
                bb_basename(e.name(), base, sizeof(base));
                snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", base);
                if (!BB_FS.exists(p)) strncpy(orphan, base, sizeof(orphan) - 1);
            }
            e.close();
        }
        dir.close();
        if (!orphan[0]) return;
        char p[64];
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", orphan);
        if (!BB_FS.remove(p)) return;
        Serial.printf("[BB] removed %s.log, its save never finished\n", orphan);
    }
}

// Highest event number on disk, so numbering carries on across a reboot
// (blackbox_name.h). Once, at boot.
static uint32_t bb_scan_last_seq() {
    File dir = BB_FS.open(BLACKBOX_DIR);
    if (!dir) return 0;
    uint32_t last = 0;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        char base[40];
        bb_basename(e.name(), base, sizeof(base));
        uint32_t s = bb_event_seq(base);
        if (s > last) last = s;
        e.close();
    }
    dir.close();
    return last;
}

static void backend_init() {
#if defined(BLACKBOX_BACKEND_LITTLEFS)
    g_fs_ok = LittleFS.begin(true);  // format-on-fail: own the spiffs data partition
    if (!g_fs_ok) { Serial.println("[BB] LittleFS mount failed"); return; }
#else
    g_fs_ok = SD.cardType() != CARD_NONE;  // can_dump_init() already mounted SD
#endif
    if (g_fs_ok && !BB_FS.exists(BLACKBOX_DIR)) BB_FS.mkdir(BLACKBOX_DIR);
    if (g_fs_ok) bb_drop_orphans();
    g_event_count = g_fs_ok ? bb_scan_count() : 0;  // one boot-time scan
    if (g_fs_ok) g_next_seq = bb_scan_last_seq() + 1u;
    Serial.printf("[BB] backend=%s ok=%d events=%d next=%lu\n",
                  BLACKBOX_BACKEND_NAME, g_fs_ok, g_event_count, (unsigned long)g_next_seq);
}

// Status/poll path: return the cache, never scan (see g_event_count).
static int backend_count() { return g_fs_ok ? g_event_count : 0; }

static String backend_list_json() {
    String out = "[";
    if (!g_fs_ok) { out += "]"; return out; }
    File dir = BB_FS.open(BLACKBOX_DIR);
    if (!dir) { out += "]"; return out; }
    bool first = true;
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        const char* nm = e.name();
        if (strstr(nm, ".json")) {
            char base[40];
            bb_basename(nm, base, sizeof(base));
            String summary;
            while (e.available()) summary += (char)e.read();
            if (!first) out += ',';
            first = false;
            out += "{\"name\":\""; out += base;
            out += "\",\"seq\":"; out += bb_event_seq(base);
            out += ",\"summary\":";
            out += summary.length() ? summary : String("null");
            out += '}';
        }
        e.close();
    }
    dir.close();
    out += "]";
    return out;
}

static bool backend_size(const char* name, bool json, size_t* out) {
    if (!g_fs_ok) return false;
    char p[64];
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.%s", name, json ? "json" : "log");
    File f = BB_OPEN_R(p);
    if (!f) return false;
    if (out) *out = f.size();
    f.close();
    return true;
}

static void backend_stream_body(WiFiClient& client, const char* name, bool json) {
    if (!g_fs_ok) return;
    char p[64];
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.%s", name, json ? "json" : "log");
    File f = BB_OPEN_R(p);
    if (!f) return;
    uint8_t buf[512];
    while (f.available()) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        client.write(buf, n);
    }
    f.close();
}

static bool backend_delete(const char* name) {
    if (!g_fs_ok) return false;
    char p[64];
    bool ok = false;
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", name);  ok |= BB_FS.remove(p);
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", name); ok |= BB_FS.remove(p);
    if (ok) g_event_count = bb_scan_count();
    return ok;
}

static void backend_delete_all() {
    if (!g_fs_ok) return;
    for (;;) {
        File dir = BB_FS.open(BLACKBOX_DIR);
        if (!dir) return;
        char victim[40] = {};
        for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
            if (strstr(e.name(), ".json")) { bb_basename(e.name(), victim, sizeof(victim)); e.close(); break; }
            e.close();
        }
        dir.close();
        if (victim[0] == '\0') return;
        backend_delete(victim);
    }
}

static bool backend_is_volatile() { return false; }

#else  // ── RAM backend (volatile, download-only) ──────────────────────────────

#include "blackbox_ram.h"  // BBRamSlot, bb_ram_reserve (one window at a time)

static bool      g_slot_used = false;
static char      g_slot_base[40] = {};
static String    g_slot_json;
static BBRamSlot g_slot = {nullptr, 0};   // the frozen frames of the kept event
static uint32_t  g_slot_n = 0;
static uint32_t  g_slot_window_start = 0;

static inline const BBFrame* g_slot_frames() { return (const BBFrame*)g_slot.buf; }

static void backend_init() {
    Serial.println("[BB] backend=ram (volatile, dashboard download only)");
}

// The RAM backend saves in one step (do_flush()), so nothing is ever kept.
static uint32_t bb_keep_slot() { return g_cap; }
static void bb_save_abort() {}

// Reserve the slot for a new event of `n` frames, freeing the previous event
// first so the save never needs two windows at once (#191). Returns the frame
// buffer to copy the window into, or nullptr on OOM (the slot is left empty).
static BBFrame* backend_store_begin(uint32_t n) {
    g_slot_used = false;   // the previous event is being replaced
    return (BBFrame*)bb_ram_reserve(&g_slot, (size_t)n * sizeof(BBFrame), malloc, free);
}

// Mark the reserved slot as a complete event once its frames are copied in.
static void backend_store_commit(const char* base, const char* json,
                                 uint32_t n, uint32_t window_start) {
    g_slot_n = n;
    g_slot_window_start = window_start;
    g_slot_json = json;
    strncpy(g_slot_base, base, sizeof(g_slot_base) - 1);
    g_slot_used = true;
}

static int backend_count() { return g_slot_used ? 1 : 0; }

static String backend_list_json() {
    String out = "[";
    if (g_slot_used) {
        out += "{\"name\":\""; out += g_slot_base;
        out += "\",\"seq\":"; out += bb_event_seq(g_slot_base);
        out += ",\"summary\":";
        out += g_slot_json.length() ? g_slot_json : String("null");
        out += '}';
    }
    out += "]";
    return out;
}

static bool backend_size(const char* name, bool json, size_t* out) {
    if (!g_slot_used || strcmp(name, g_slot_base) != 0) return false;
    if (json) { if (out) *out = g_slot_json.length(); return true; }
    // .log: sum the formatted candump line lengths (no large RAM text copy).
    size_t total = 0;
    char line[72];
    const BBFrame* frames = g_slot_frames();
    for (uint32_t i = 0; i < g_slot_n; i++) {
        const BBFrame& f = frames[i];
        total += tesla_format_candump_line(line, sizeof(line), f.ts_ms - g_slot_window_start,
                                           bb_iface_name(f),
                                           f.id, f.data, f.dlc);
    }
    if (out) *out = total;
    return true;
}

static void backend_stream_body(WiFiClient& client, const char* name, bool json) {
    if (!g_slot_used || strcmp(name, g_slot_base) != 0) return;
    if (json) { client.print(g_slot_json); return; }
    char line[72];
    const BBFrame* frames = g_slot_frames();
    for (uint32_t i = 0; i < g_slot_n; i++) {
        const BBFrame& f = frames[i];
        int n = tesla_format_candump_line(line, sizeof(line), f.ts_ms - g_slot_window_start,
                                          bb_iface_name(f),
                                          f.id, f.data, f.dlc);
        client.write((const uint8_t*)line, n);
    }
}

static bool backend_delete(const char* name) {
    if (!g_slot_used || strcmp(name, g_slot_base) != 0) return false;
    if (g_slot.buf) { free(g_slot.buf); g_slot.buf = nullptr; g_slot.bytes = 0; }
    g_slot_n = 0;
    g_slot_used = false;
    g_slot_json = String();
    return true;
}

static void backend_delete_all() { if (g_slot_used) backend_delete(g_slot_base); }

static bool backend_is_volatile() { return true; }

#endif  // backend

// ─────────────────────────────────────────────────────────────────────────────
//  Common: ring + trigger + flush
// ─────────────────────────────────────────────────────────────────────────────
// Keep this much heap/PSRAM free after the ring so the WiFi/web stack always
// has a working margin. The ~108 KB S3 ring grabbed at boot could starve WiFi
// → OOM reboot loop (#124); the guard makes an on-demand enable refuse rather
// than brick connectivity.
#define BLACKBOX_HEAP_MARGIN  (60u * 1024u)

// Try to allocate the ring in the requested memory, but only if enough stays
// free afterwards. Returns true and publishes g_ring/g_cap on success.
static bool bb_try_alloc(bool psram, uint32_t frames) {
    size_t bytes = (size_t)frames * sizeof(BBFrame);
    size_t freeb = psram ? ESP.getFreePsram() : ESP.getFreeHeap();
    if (freeb < bytes + BLACKBOX_HEAP_MARGIN) {
        Serial.printf("[BB] ring alloc skipped (%s): free %luB < need %luB + %luB margin\n",
                      psram ? "psram" : "heap", (unsigned long)freeb,
                      (unsigned long)bytes, (unsigned long)BLACKBOX_HEAP_MARGIN);
        return false;
    }
    BBFrame* p = (BBFrame*)(psram ? ps_malloc(bytes) : malloc(bytes));
    if (!p) { Serial.printf("[BB] ring alloc failed (%s)\n", psram ? "psram" : "heap"); return false; }
    g_ring = p;
    g_cap = frames;
    g_ring_psram = psram;
    g_head = g_tail = 0;
    Serial.printf("[BB] ring=%lu frames (%lu KB) %s  window=%us pre/%us post\n",
                  (unsigned long)g_cap, (unsigned long)(bytes / 1024u),
                  g_ring_psram ? "PSRAM" : "internal-RAM",
                  BLACKBOX_PRE_MS / 1000u, BLACKBOX_POST_MS / 1000u);
    return true;
}

// Lazily allocate the ring (on enable). PSRAM first when present, else internal
// RAM — each attempt is heap-guarded. Returns true once g_ring is ready.
static bool blackbox_alloc_ring() {
    if (g_ring) return true;
    if (g_want_psram && bb_try_alloc(true, BLACKBOX_FRAMES_PSRAM)) return true;
    // Internal-RAM fallback: size the ring to the heap we actually have, not a
    // fixed BLACKBOX_FRAMES_INTERNAL. A classic ESP32 with no PSRAM has only
    // ~80-90 KB free after the WiFi AP + web server, so the fixed 57 KB ring
    // plus the 60 KB margin (117 KB) could never be satisfied: enable was always
    // refused and the dashboard toggle bounced straight back off (#191). Take the
    // largest ring that still leaves the margin free, clamped to the floor
    // (~4 s window) and the per-target save-copy cap — a shorter window on a
    // low-RAM board, which still records, beats no black box at all. bb_try_alloc
    // re-guards against the live heap and logs the chosen frame count / KB.
    uint32_t frames = bb_ram_ring_frames(ESP.getFreeHeap(), BLACKBOX_HEAP_MARGIN,
                                         sizeof(BBFrame), BB_MIN_INTERNAL_FRAMES,
                                         BLACKBOX_FRAMES_INTERNAL);
    if (frames == 0u) return false;   // margin + floor won't fit: genuinely too tight
    return bb_try_alloc(false, frames);
}

// Free the ring and drop any pre-roll / armed capture (on disable).
static void blackbox_free_ring() {
    bb_save_abort();
    if (g_ring) { free(g_ring); g_ring = nullptr; }
    g_cap = 0;
    g_head = g_tail = 0;
    g_armed = false;
    g_queued = false;
}

void blackbox_init(FSDState* state, portMUX_TYPE* state_mux) {
    g_state = state;
    g_mux   = state_mux;

    // Size decision only — the ring is allocated lazily on enable so boot never
    // competes with the WiFi/web stack for heap (#124).
    g_want_psram = ESP.getPsramSize() > 0;
    g_ring = nullptr;
    g_cap = 0;
    g_ring_psram = false;

    backend_init();
    Serial.printf("[BB] init backend=%s — ring allocated on enable "
                  "(%s target %lu frames)\n",
                  BLACKBOX_BACKEND_NAME,
                  g_want_psram ? "PSRAM" : "internal-RAM",
                  (unsigned long)(g_want_psram ? BLACKBOX_FRAMES_PSRAM
                                               : BLACKBOX_FRAMES_INTERNAL));
}

// Common ring-write path shared by the RX and TX entry points. `is_tx` tags the
// stored frame; everything else — id filter, eviction, memcpy — is identical, so
// TX frames pass the SAME key-id filter as RX and can't flood the ring with the
// high-rate nag echo.
static void bb_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms, uint8_t is_tx) {
    if (g_cap == 0 || g_state == nullptr || !g_state->blackbox_enabled) return;
    // Store only the key diagnostic IDs (fsd_blackbox_filter.h). On a busy full
    // bus (~3300 f/s) recording everything fills the ring in ~1.8 s, truncating
    // the 10 s pre / 5 s post window; the filter drops the stored rate ~15x so
    // the whole window survives. Define BLACKBOX_CAPTURE_ALL to keep every frame.
    if (!fsd_blackbox_should_record(frame.id)) return;
    // The internal-RAM ring keeps 0x257 at 10 Hz per bus; PSRAM keeps every frame.
    if (!g_ring_psram &&
        !fsd_blackbox_thin_keep(frame.id, now_ms, &g_thin[bus < CAN_BUS_COUNT ? bus : 0]))
        return;
    uint32_t slot;
    // Evicts the oldest frame, or drops this one if that is still to be saved.
    if (!bb_ring_push(g_cap, g_head, g_tail, bb_keep_slot(), &slot)) return;
    BBFrame& s = g_ring[slot];
    s.ts_ms = now_ms;
    s.id    = frame.id;
    s.bus   = (uint8_t)bus;
    s.dlc   = frame.dlc > 8 ? 8 : frame.dlc;
    s.is_tx = is_tx;
    memcpy(s.data, frame.data, s.dlc);
    if (s.dlc < 8) memset(s.data + s.dlc, 0, 8 - s.dlc);
}

void blackbox_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms) {
    bb_record(bus, frame, now_ms, 0);   // RX (bus) frame
}

void blackbox_record_tx(CanBusId bus, const CanFrame& frame, uint32_t now_ms) {
    bb_record(bus, frame, now_ms, 1);   // TX (our injected/echoed) frame
}

void blackbox_note_ap_state(uint8_t ap_state, uint32_t now_ms) {
    if (ap_state == g_tl_last) return;
    g_tl_last = ap_state;
    g_tl_ts[g_tl_head] = now_ms;
    g_tl_state[g_tl_head] = ap_state;
    g_tl_head = (g_tl_head + 1u) % BLACKBOX_TL_MAX;
    if (g_tl_count < BLACKBOX_TL_MAX) g_tl_count++;
}

void blackbox_arm(BBTrigger trig, const FSDState* snap, uint32_t now_ms) {
    if (g_cap == 0 || g_state == nullptr || !g_state->blackbox_enabled) return;
    switch (bb_arm_action(g_armed, g_trig_ms, g_queued, g_q_ms, BLACKBOX_POST_MS, now_ms)) {
    case BB_ARM_IN_WINDOW:  // already in the window of an armed or waiting event
        return;
    case BB_ARM_QUEUE:      // the armed event is being saved: this one is next
        g_queued = true;
        g_q_trig = trig;
        g_q_ms = now_ms;
        if (snap) g_q_snap = *snap; else g_q_snap = *g_state;
        Serial.printf("[BB] %s @ %lums waits for the save in progress\n",
                      trig_name_uc(trig), (unsigned long)now_ms);
        return;
    case BB_ARM_MISSED:     // one already waits, and this is past its window
        g_missed++;
        Serial.printf("[BB] %s @ %lums not recorded: a save is running and another "
                      "event waits\n", trig_name_uc(trig), (unsigned long)now_ms);
        return;
    case BB_ARM_NOW:
    default:
        break;
    }
    g_armed = true;
    g_trig = trig;
    g_trig_ms = now_ms;
    g_flush_at_ms = now_ms + BLACKBOX_POST_MS;
    if (snap) g_snap = *snap; else g_snap = *g_state;
    Serial.printf("[BB] armed by %s @ %lums — post-roll %ums\n",
                  trig_name_uc(trig), (unsigned long)now_ms, BLACKBOX_POST_MS);
}

void blackbox_busoff(uint32_t now_ms) {
    if (g_state == nullptr) return;
    FSDState snap;
    bb_enter();
    FSDEventType e = fsd_events_inject(g_state, EVT_BUSOFF, now_ms);
    snap = *g_state;
    bb_exit();
    if (e == EVT_BUSOFF) blackbox_arm(BB_TRIG_BUSOFF, &snap, now_ms);
}

void blackbox_mark(uint32_t now_ms) {
    if (g_state == nullptr) return;
    FSDState snap;
    bb_enter();
    FSDEventType e = fsd_events_inject(g_state, EVT_MANUAL, now_ms);
    snap = *g_state;
    bb_exit();
    if (e == EVT_MANUAL) blackbox_arm(BB_TRIG_MANUAL, &snap, now_ms);
    else Serial.println("[BB] mark suppressed (cooldown)");
}

// Build the .json summary from g_snap + the windowed frame/bus counts + timeline.
static void build_summary(char* out, int out_sz, uint32_t frame_count,
                          uint32_t window_start, uint32_t bus0, uint32_t bus1) {
    uint32_t tl_ts[BLACKBOX_TL_EMIT];
    uint8_t  tl_state[BLACKBOX_TL_EMIT];
    int tlc = 0;
    uint32_t lo = (g_trig_ms >= BLACKBOX_PRE_MS) ? g_trig_ms - BLACKBOX_PRE_MS : 0u;
    uint32_t hi = g_trig_ms + BLACKBOX_POST_MS;
    // Walk the timeline ring oldest→newest, keep entries inside the window.
    uint32_t start = (g_tl_count < BLACKBOX_TL_MAX) ? 0u
                     : g_tl_head;  // wrapped: oldest is at head
    for (uint32_t k = 0; k < g_tl_count && tlc < BLACKBOX_TL_EMIT; k++) {
        uint32_t idx = (start + k) % BLACKBOX_TL_MAX;
        uint32_t ts = g_tl_ts[idx];
        if (ts < lo || ts > hi) continue;
        // window_start is the first frame in-window, which can post-date an
        // entry (or the trigger) on a sparse/idle bus — clamp so the rel-ms
        // never underflows to ~2^32.
        tl_ts[tlc] = (ts >= window_start) ? ts - window_start : 0u;
        tl_state[tlc] = g_tl_state[idx];
        tlc++;
    }

    FSDBlackboxSummary s;
    memset(&s, 0, sizeof(s));
    s.trigger        = trig_name_uc(g_trig);
    s.from_state     = g_snap.evt_last_from;
    s.to_state       = g_snap.evt_last_to;
    // When the window's first frame post-dates the trigger (idle bus, trigger
    // fires before any post-roll frame lands) window_start > g_trig_ms; guard
    // the unsigned subtraction so t= reads 0 instead of underflowing to ~2^32.
    s.trigger_rel_ms = (g_trig_ms >= window_start) ? g_trig_ms - window_start : 0u;
    s.window_pre_ms  = BLACKBOX_PRE_MS;
    s.window_post_ms = BLACKBOX_POST_MS;
    s.frame_count    = frame_count;
    s.hw_version     = (int)g_snap.hw_version;
    s.hw4_das_status_seen = g_snap.das_hw4_status_seen;
#if defined(CAN_DRIVER_T2CAN_DUAL)
    s.dual_can       = true;
#else
    s.dual_can       = false;
#endif
    s.bus0_frames    = bus0;
    s.bus1_frames    = bus1;
    s.nag            = g_snap.nag_killer;
    s.ap_first       = g_snap.ap_first;
    s.abort_guard    = g_snap.abort_guard;
    s.signal_map     = (g_snap.cfg_das_id != 0);
    s.nag_burst      = g_snap.nag_burst;
    s.tl_ts          = tl_ts;
    s.tl_state       = tl_state;
    s.tl_count       = tlc;
    fsd_blackbox_format_json(out, out_sz, &s);
}

// The event's .json: the shared summary plus a "tx_frames" count. The shared
// summary formatter (fsd_logic, pure) is a per-event summary, not a per-frame
// record list, so direction is reported here as a count: splice a "tx_frames"
// field in before the closing brace so a decoded summary tells you how many of
// the frames were ours. Per-frame direction lives in the .log via the
// can0TX/can1TX interface tag.
static void bb_event_json(char* json, int json_sz, uint32_t count, uint32_t window_start,
                          uint32_t bus0, uint32_t bus1, uint32_t txc) {
    build_summary(json, json_sz, count, window_start, bus0, bus1);
    size_t jl = strlen(json);
    if (jl > 0 && json[jl - 1] == '}' && jl + 24 < (size_t)json_sz)
        snprintf(json + jl - 1, (size_t)json_sz - (jl - 1),
                 ",\"tx_frames\":%lu}", (unsigned long)txc);
}

#if defined(BLACKBOX_BACKEND_LITTLEFS) || defined(BLACKBOX_BACKEND_SD)
// One step of saving the armed event; true once the save is over. Each step
// is one file operation or one slice of .log lines, and blackbox_tick() runs
// the next one *gap_ms later, so the loop never waits for a whole event to
// reach the disk. The ring keeps recording; bb_keep_slot() protects the
// frames not written yet.
static bool do_flush(uint32_t now_ms, uint32_t* gap_ms) {
    char p[64];
    switch (g_save.step) {
    case BB_SAVE_IDLE: {
        if (!g_fs_ok) return true;
        uint32_t lo = (g_trig_ms >= BLACKBOX_PRE_MS) ? g_trig_ms - BLACKBOX_PRE_MS : 0u;
        uint32_t hi = g_trig_ms + BLACKBOX_POST_MS;
        BBWindow w = bb_find_window(bb_ring_count(g_cap, g_head, g_tail), lo, hi,
                                    [](uint32_t k) { return g_ring[(g_tail + k) % g_cap].ts_ms; });
        g_save.next  = (g_tail + w.first) % g_cap;
        g_save.left  = w.count;
        g_save.count = w.count;
        g_save.window_start = w.count ? g_ring[g_save.next].ts_ms : g_trig_ms;
        g_save.bus0 = g_save.bus1 = g_save.txc = 0;
        g_save.t0 = now_ms;
        bb_event_name(g_save.base, sizeof(g_save.base), g_next_seq++, trig_name(g_trig));
        g_save.step = BB_SAVE_ROOM;
        return false;
    }
    case BB_SAVE_ROOM: {
#if defined(BLACKBOX_BACKEND_LITTLEFS)
        // Plus the .json (under 1 KB) and a block per file: LittleFS stores
        // files in whole 4 KB blocks. One old event is dropped per step.
        size_t need = (size_t)g_save.count * BLACKBOX_LINE_MAX + 1024u + 2u * 4096u;
        if (bb_fs_free() < need + BLACKBOX_FS_RESERVE) {
            if (bb_drop_oldest(0)) return false;
            g_event_count = bb_scan_count();
            Serial.printf("[BB] %s not saved: needs %lu KB + %lu KB reserve, %lu KB free\n",
                          g_save.base, (unsigned long)(need / 1024u),
                          (unsigned long)(BLACKBOX_FS_RESERVE / 1024u),
                          (unsigned long)(bb_fs_free() / 1024u));
            bb_save_abort();
            return true;
        }
#endif
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", g_save.base);
        g_save.log = BB_OPEN_W(p);
        if (!g_save.log) {
            Serial.printf("[BB] .log open failed: %s\n", p);
            bb_save_abort();
            return true;
        }
        g_save.step = BB_SAVE_LOG;
        return false;
    }
    case BB_SAVE_LOG: {
        if (g_save.left == 0) {  // every line written; closing is a step of its own
            g_save.log.close();
            g_save.step = BB_SAVE_JSON;
            return false;
        }
        char line[72];
        const uint32_t n = bb_slice(g_save.left, BLACKBOX_FLUSH_SLICE);
        const uint32_t t_us = micros();
        uint32_t lines = 0;
        do {
            const BBFrame& fr = g_ring[g_save.next];
            int len = tesla_format_candump_line(line, sizeof(line), fr.ts_ms - g_save.window_start,
                                                bb_iface_name(fr), fr.id, fr.data, fr.dlc);
            g_save.log.write((const uint8_t*)line, len);
            if (fr.is_tx) g_save.txc++;
            if (fr.bus == CAN_BUS_SECONDARY) g_save.bus1++; else g_save.bus0++;
            g_save.next = ring_next(g_save.next);
            g_save.left--;
            lines++;
        } while (!bb_slice_over(lines, n, micros() - t_us, BLACKBOX_FLUSH_BUDGET_US));
        *gap_ms = bb_slice_gap_ms(lines, BLACKBOX_FLUSH_SLICE, BLACKBOX_FLUSH_GAP_MS);
        return false;
    }
    case BB_SAVE_JSON: {
        char json[640];
        bb_event_json(json, sizeof(json), g_save.count, g_save.window_start,
                      g_save.bus0, g_save.bus1, g_save.txc);
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", g_save.base);
        File jf = BB_OPEN_W(p);
        if (!jf) {
            Serial.printf("[BB] .json open failed: %s\n", p);
            bb_save_abort();
            return true;
        }
        jf.write((const uint8_t*)json, strlen(json));
        jf.close();
        g_save.step = BB_SAVE_TIDY;
        return false;
    }
    case BB_SAVE_TIDY:
    default:
        bb_enforce_retention();
        g_event_count = bb_scan_count();  // the new event + any retention drop
        g_captures++;
        Serial.printf("[BB] flushed %s  frames=%lu (can0=%lu can1=%lu) in %lu ms\n",
                      g_save.base, (unsigned long)g_save.count, (unsigned long)g_save.bus0,
                      (unsigned long)g_save.bus1, (unsigned long)(now_ms - g_save.t0));
        g_save.step = BB_SAVE_IDLE;
        return true;
    }
}
#else
// RAM: freeze the windowed frames into a contiguous buffer for download, in
// one step. The window is copied STRAIGHT into the slot buffer (one allocation,
// one window of RAM) — no scratch copy held alongside it, so a save needs about
// half the transient RAM the two-copy path did (#191).
static bool do_flush(uint32_t now_ms, uint32_t* gap_ms) {
    (void)gap_ms;
    uint32_t lo = (g_trig_ms >= BLACKBOX_PRE_MS) ? g_trig_ms - BLACKBOX_PRE_MS : 0u;
    uint32_t hi = g_trig_ms + BLACKBOX_POST_MS;

    // Pass 1: window start + per-bus counts + injected (TX) frame count.
    uint32_t count = 0, bus0 = 0, bus1 = 0, window_start = 0, txc = 0;
    bool started = false;
    for (uint32_t i = g_tail; i != g_head; i = ring_next(i)) {
        const BBFrame& fr = g_ring[i];
        if (fr.ts_ms < lo) continue;
        if (fr.ts_ms > hi) break;
        if (!started) { window_start = fr.ts_ms; started = true; }
        count++;
        if (fr.is_tx) txc++;
        if (fr.bus == CAN_BUS_SECONDARY) bus1++; else bus0++;
    }
    if (!started) window_start = g_trig_ms;

    char base[40];
    bb_event_name(base, sizeof(base), g_next_seq++, trig_name(g_trig));

    char json[640];
    bb_event_json(json, sizeof(json), count, window_start, bus0, bus1, txc);

    // One window of RAM: the previous event is freed before this one is taken,
    // then the ring window is copied directly into the slot.
    BBFrame* dst = backend_store_begin(count);
    if (!dst) {
        // A real OOM: tell the dashboard instead of leaving a silent empty list.
        g_save_lowmem = true;
        Serial.printf("[BB] %s not saved: low memory (need %lu frames, %lu B free)\n",
                      base, (unsigned long)count,
                      (unsigned long)ESP.getFreeHeap());
        return true;
    }
    uint32_t fn = 0;
    for (uint32_t i = g_tail; i != g_head && fn < count; i = ring_next(i)) {
        const BBFrame& fr = g_ring[i];
        if (fr.ts_ms < lo) continue;
        if (fr.ts_ms > hi) break;
        dst[fn++] = fr;
    }
    backend_store_commit(base, json, fn, window_start);
    g_save_lowmem = false;
    g_captures++;
    Serial.printf("[BB] flushed %s  frames=%lu (can0=%lu can1=%lu) in %lu ms\n",
                  base, (unsigned long)count, (unsigned long)bus0, (unsigned long)bus1,
                  (unsigned long)(millis() - now_ms));
    return true;
}
#endif

void blackbox_tick(uint32_t now_ms) {
    if (!g_armed) return;
    if ((int32_t)(now_ms - g_flush_at_ms) < 0) return;  // post-roll, or between save steps
    uint32_t gap_ms = BLACKBOX_FLUSH_GAP_MS;
    if (!do_flush(now_ms, &gap_ms)) {
        g_flush_at_ms = now_ms + gap_ms;
        return;
    }
    g_armed = false;
    if (!g_queued) return;
    // The trigger that came during the save: its window, its snapshot. With its
    // post-roll already over, its save starts on the next call.
    g_queued = false;
    g_armed = true;
    g_trig = g_q_trig;
    g_trig_ms = g_q_ms;
    g_flush_at_ms = g_q_ms + BLACKBOX_POST_MS;
    g_snap = g_q_snap;
    Serial.printf("[BB] armed by %s @ %lums, after the save\n",
                  trig_name_uc(g_trig), (unsigned long)g_trig_ms);
}

void blackbox_set_enabled(bool enabled) {
    if (g_state == nullptr) return;
    if (enabled) {
        // Allocate the ring on demand, heap-guarded. If it can't be had without
        // starving WiFi/web, stay disabled and report it — never brick the link.
        if (!blackbox_alloc_ring()) {
            bb_enter();
            g_state->blackbox_enabled = false;
            bb_exit();
            Serial.println("[BB] enable refused — not enough free heap; staying off");
            return;
        }
        bb_enter();
        g_state->blackbox_enabled = true;
        bb_exit();
        Serial.println("[BB] enabled");
    } else {
        bb_enter();
        g_state->blackbox_enabled = false;
        bb_exit();
        blackbox_free_ring();  // stop + drop pre-roll + release the ring
        Serial.println("[BB] disabled");
    }
}

// Reports true only when the ring is actually live, so a guard-refused enable
// (or a persisted-ON boot before reconcile) shows as off on the dashboard.
bool blackbox_is_enabled() {
    return g_state != nullptr && g_state->blackbox_enabled && g_ring != nullptr;
}

String blackbox_status_json() {
    String j = "{";
    j += "\"enabled\":";  j += blackbox_is_enabled() ? "true" : "false"; j += ',';
    j += "\"backend\":\""; j += BLACKBOX_BACKEND_NAME; j += "\",";
    j += "\"volatile\":"; j += backend_is_volatile() ? "true" : "false"; j += ',';
    j += "\"psram\":";    j += g_ring_psram ? "true" : "false"; j += ',';
    j += "\"cap\":";      j += g_cap; j += ',';
    j += "\"armed\":";    j += g_armed ? "true" : "false"; j += ',';
    j += "\"queued\":";   j += g_queued ? "true" : "false"; j += ',';
    j += "\"missed\":";   j += g_missed; j += ',';
    j += "\"events\":";   j += backend_count(); j += ',';
    j += "\"save_lowmem\":"; j += g_save_lowmem ? "true" : "false"; j += ',';
    j += "\"captures\":"; j += g_captures;
    j += '}';
    return j;
}

String blackbox_list_json() { return backend_list_json(); }

bool blackbox_file_size(const char* name, bool json, size_t* size_out) {
    if (!bb_name_ok(name)) return false;
    return backend_size(name, json, size_out);
}

void blackbox_stream_body(WiFiClient& client, const char* name, bool json) {
    if (!bb_name_ok(name)) return;
    backend_stream_body(client, name, json);
}

bool blackbox_delete(const char* name) {
    if (!bb_name_ok(name)) return false;
    return backend_delete(name);
}

void blackbox_delete_all() { backend_delete_all(); }
