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
// Ring capacity in frames @ 18 B/frame. PSRAM (runtime-detected) holds a full
// ~15 s window at a busy bus rate. The internal-RAM fallback must cover the
// window on its own — PSRAM is never required and no build enables it:
//   - S3-class (512 KB SRAM): 6000 frames ≈ 108 KB → ≥15 s even at ~400 f/s.
//     Only paired with the persistent disk backends here, which stream the ring
//     straight to file (no frozen copy), so the ring is the whole footprint.
//   - Classic ESP32: 3000 frames ≈ 54 KB safety cap. These are the volatile
//     RAM-backend boards (default OFF) and a flush also frees+allocs a frozen
//     copy of the in-window frames, so the cap bounds the transient peak.
#ifndef BLACKBOX_FRAMES_PSRAM
#define BLACKBOX_FRAMES_PSRAM    40000u   // ~720 KB
#endif
#ifndef BLACKBOX_FRAMES_INTERNAL
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define BLACKBOX_FRAMES_INTERNAL  6000u   // ~108 KB (S3, 512 KB SRAM)
#else
#define BLACKBOX_FRAMES_INTERNAL  3000u   // ~54 KB  (classic ESP32)
#endif
#endif

#define BLACKBOX_DIR        "/blackbox"
#define BLACKBOX_RETAIN      5u      // keep newest N events (disk backends)
#define BLACKBOX_TL_MAX      16      // das_ap_state timeline depth
#define BLACKBOX_TL_EMIT     12      // timeline entries written to the summary

struct __attribute__((packed)) BBFrame {
    uint32_t ts_ms;
    uint32_t id;
    uint8_t  bus;
    uint8_t  dlc;
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

static bool      g_armed = false;
static BBTrigger g_trig = BB_TRIG_ABORT;
static uint32_t  g_trig_ms = 0;
static uint32_t  g_flush_at_ms = 0;
static FSDState  g_snap;            // FSDState at trigger time (toggles/hw/evt_last_*)
static uint32_t  g_captures = 0;    // monotonic count since boot (badge source)

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

static uint32_t bb_seq_from_name(const char* base) {
    // evt_<ms>_<trigger>  → parse <ms>
    const char* u = strchr(base, '_');
    return u ? (uint32_t)strtoul(u + 1, nullptr, 10) : 0u;
}

// Keep only the newest BLACKBOX_RETAIN events (smallest <ms> deleted first).
static void bb_enforce_retention() {
    for (;;) {
        File dir = BB_FS.open(BLACKBOX_DIR);
        if (!dir) return;
        uint32_t count = 0, oldest = 0xFFFFFFFFu;
        char oldest_base[40] = {};
        for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
            const char* nm = e.name();
            if (strstr(nm, ".json")) {
                char base[40];
                bb_basename(nm, base, sizeof(base));
                uint32_t s = bb_seq_from_name(base);
                count++;
                if (s < oldest) { oldest = s; strncpy(oldest_base, base, sizeof(oldest_base) - 1); }
            }
            e.close();
        }
        dir.close();
        if (count <= BLACKBOX_RETAIN || oldest_base[0] == '\0') return;
        char p[64];
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", oldest_base);  BB_FS.remove(p);
        snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", oldest_base); BB_FS.remove(p);
    }
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

static void backend_init() {
#if defined(BLACKBOX_BACKEND_LITTLEFS)
    g_fs_ok = LittleFS.begin(true);  // format-on-fail: own the spiffs data partition
    if (!g_fs_ok) { Serial.println("[BB] LittleFS mount failed"); return; }
#else
    g_fs_ok = SD.cardType() != CARD_NONE;  // can_dump_init() already mounted SD
#endif
    if (g_fs_ok && !BB_FS.exists(BLACKBOX_DIR)) BB_FS.mkdir(BLACKBOX_DIR);
    g_event_count = g_fs_ok ? bb_scan_count() : 0;  // one boot-time scan
    Serial.printf("[BB] backend=%s ok=%d events=%d\n",
                  BLACKBOX_BACKEND_NAME, g_fs_ok, g_event_count);
}

// Persist one event. `frame_count`/per-bus counts already computed by the caller;
// `write` is invoked to stream candump lines into the open .log.
static void backend_store(const char* base, const char* json,
                          void (*emit)(File&)) {
    if (!g_fs_ok) return;
    char p[64];
    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.log", base);
    File lf = BB_OPEN_W(p);
    if (lf) { emit(lf); lf.flush(); lf.close(); }
    else Serial.printf("[BB] .log open failed: %s\n", p);

    snprintf(p, sizeof(p), BLACKBOX_DIR "/%s.json", base);
    File jf = BB_OPEN_W(p);
    if (jf) { jf.write((const uint8_t*)json, strlen(json)); jf.flush(); jf.close(); }
    else Serial.printf("[BB] .json open failed: %s\n", p);

    bb_enforce_retention();
    g_event_count = bb_scan_count();  // reflects the new event + any retention drop
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
            out += "{\"name\":\""; out += base; out += "\",\"summary\":";
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

static bool      g_slot_used = false;
static char      g_slot_base[40] = {};
static String    g_slot_json;
static BBFrame*  g_slot_frames = nullptr;
static uint32_t  g_slot_n = 0;
static uint32_t  g_slot_window_start = 0;

static void backend_init() {
    Serial.println("[BB] backend=ram (volatile, dashboard download only)");
}

static void backend_store_ram(const char* base, const char* json,
                              const BBFrame* frames, uint32_t n, uint32_t window_start) {
    if (g_slot_frames) { free(g_slot_frames); g_slot_frames = nullptr; }
    g_slot_used = false;
    g_slot_frames = (BBFrame*)malloc((size_t)n * sizeof(BBFrame));
    if (!g_slot_frames) { Serial.println("[BB] RAM event alloc failed"); return; }
    memcpy(g_slot_frames, frames, (size_t)n * sizeof(BBFrame));
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
        out += "{\"name\":\""; out += g_slot_base; out += "\",\"summary\":";
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
    for (uint32_t i = 0; i < g_slot_n; i++) {
        const BBFrame& f = g_slot_frames[i];
        total += tesla_format_candump_line(line, sizeof(line), f.ts_ms - g_slot_window_start,
                                           f.bus == CAN_BUS_SECONDARY ? "can1" : "can0",
                                           f.id, f.data, f.dlc);
    }
    if (out) *out = total;
    return true;
}

static void backend_stream_body(WiFiClient& client, const char* name, bool json) {
    if (!g_slot_used || strcmp(name, g_slot_base) != 0) return;
    if (json) { client.print(g_slot_json); return; }
    char line[72];
    for (uint32_t i = 0; i < g_slot_n; i++) {
        const BBFrame& f = g_slot_frames[i];
        int n = tesla_format_candump_line(line, sizeof(line), f.ts_ms - g_slot_window_start,
                                          f.bus == CAN_BUS_SECONDARY ? "can1" : "can0",
                                          f.id, f.data, f.dlc);
        client.write((const uint8_t*)line, n);
    }
}

static bool backend_delete(const char* name) {
    if (!g_slot_used || strcmp(name, g_slot_base) != 0) return false;
    if (g_slot_frames) { free(g_slot_frames); g_slot_frames = nullptr; }
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
    return bb_try_alloc(false, BLACKBOX_FRAMES_INTERNAL);
}

// Free the ring and drop any pre-roll / armed capture (on disable).
static void blackbox_free_ring() {
    if (g_ring) { free(g_ring); g_ring = nullptr; }
    g_cap = 0;
    g_head = g_tail = 0;
    g_armed = false;
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

void blackbox_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms) {
    if (g_cap == 0 || g_state == nullptr || !g_state->blackbox_enabled) return;
    if (ring_next(g_head) == g_tail) g_tail = ring_next(g_tail);  // evict oldest
    BBFrame& s = g_ring[g_head];
    s.ts_ms = now_ms;
    s.id    = frame.id;
    s.bus   = (uint8_t)bus;
    s.dlc   = frame.dlc > 8 ? 8 : frame.dlc;
    memcpy(s.data, frame.data, s.dlc);
    if (s.dlc < 8) memset(s.data + s.dlc, 0, 8 - s.dlc);
    g_head = ring_next(g_head);
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
    if (g_armed) return;  // already capturing; ignore until the post-roll flushes
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

#if defined(BLACKBOX_BACKEND_LITTLEFS) || defined(BLACKBOX_BACKEND_SD)
// Disk emit: walk the ring window and stream candump lines into the open file.
static uint32_t g_emit_lo, g_emit_hi, g_emit_start;
static void disk_emit(File& f) {
    char line[72];
    bool started = false;
    for (uint32_t i = g_tail; i != g_head; i = ring_next(i)) {
        const BBFrame& fr = g_ring[i];
        if (fr.ts_ms < g_emit_lo) continue;
        if (fr.ts_ms > g_emit_hi) break;
        if (!started) { g_emit_start = fr.ts_ms; started = true; }
        int n = tesla_format_candump_line(line, sizeof(line), fr.ts_ms - g_emit_start,
                                          fr.bus == CAN_BUS_SECONDARY ? "can1" : "can0",
                                          fr.id, fr.data, fr.dlc);
        f.write((const uint8_t*)line, n);
    }
}
#endif

static void do_flush() {
    uint32_t lo = (g_trig_ms >= BLACKBOX_PRE_MS) ? g_trig_ms - BLACKBOX_PRE_MS : 0u;
    uint32_t hi = g_trig_ms + BLACKBOX_POST_MS;

    // Pass 1: window start + per-bus counts.
    uint32_t count = 0, bus0 = 0, bus1 = 0, window_start = 0;
    bool started = false;
    for (uint32_t i = g_tail; i != g_head; i = ring_next(i)) {
        const BBFrame& fr = g_ring[i];
        if (fr.ts_ms < lo) continue;
        if (fr.ts_ms > hi) break;
        if (!started) { window_start = fr.ts_ms; started = true; }
        count++;
        if (fr.bus == CAN_BUS_SECONDARY) bus1++; else bus0++;
    }
    if (!started) window_start = g_trig_ms;

    char base[40];
    snprintf(base, sizeof(base), "evt_%lu_%s", (unsigned long)g_trig_ms, trig_name(g_trig));

    char json[640];
    build_summary(json, sizeof(json), count, window_start, bus0, bus1);

#if defined(BLACKBOX_BACKEND_LITTLEFS) || defined(BLACKBOX_BACKEND_SD)
    g_emit_lo = lo; g_emit_hi = hi;
    backend_store(base, json, disk_emit);
#else
    // RAM: freeze the windowed frames into a contiguous buffer for download.
    BBFrame* frozen = (BBFrame*)malloc((size_t)(count ? count : 1) * sizeof(BBFrame));
    uint32_t fn = 0;
    if (frozen) {
        for (uint32_t i = g_tail; i != g_head && fn < count; i = ring_next(i)) {
            const BBFrame& fr = g_ring[i];
            if (fr.ts_ms < lo) continue;
            if (fr.ts_ms > hi) break;
            frozen[fn++] = fr;
        }
        backend_store_ram(base, json, frozen, fn, window_start);
        free(frozen);
    } else {
        Serial.println("[BB] flush alloc failed");
    }
#endif
    g_captures++;
    Serial.printf("[BB] flushed %s  frames=%lu (can0=%lu can1=%lu)\n",
                  base, (unsigned long)count, (unsigned long)bus0, (unsigned long)bus1);
}

void blackbox_tick(uint32_t now_ms) {
    if (!g_armed) return;
    if ((int32_t)(now_ms - g_flush_at_ms) < 0) return;  // post-roll still running
    do_flush();
    g_armed = false;
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
    j += "\"events\":";   j += backend_count(); j += ',';
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
