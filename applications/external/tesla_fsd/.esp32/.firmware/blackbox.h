#pragma once
/*
 * blackbox.h — black-box incident recorder (ESP32, #124).
 *
 * A RAM ring buffer continuously records raw CAN frames — by default only the
 * key diagnostic ids (fsd_blackbox_filter.h), across all buses, so a busy full
 * bus (~3300 f/s) can't fill the small ring in ~1.8 s and truncate the window;
 * define BLACKBOX_CAPTURE_ALL to record every id instead. When the shared
 * event-core (fsd_events.h) reports an abort, a
 * bus-off, or a dashboard "Mark", the recorder freezes a window of pre+post
 * frames and writes two files: a pure candump .log (drops straight into the
 * CRC cracker) and a decoded .json summary (fsd_blackbox_summary.h).
 *
 * The ring lives in PSRAM when present (large window) and falls back to a
 * smaller internal-RAM window otherwise — PSRAM is never required.
 *
 * Storage is one of three backends chosen at compile time by board (see
 * blackbox.cpp): LittleFS (real data partition, persistent, retention N=5),
 * SD (LILYGO), or volatile RAM + dashboard download (min_spiffs boards).
 *
 * Default OFF; the enable toggle is persisted in NVS. The ring is allocated
 * lazily (heap-guarded) on enable — boot never grabs it, so WiFi/web always
 * get the heap first. Single-threaded: every entry point is called from the
 * Arduino loop()/CAN path.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "can_driver.h"   // CanBusId, CanFrame
#include "fsd_handler.h"  // FSDState

// ── Storage backend selection (single source of truth) ───────────────────────
// Chosen by board, like can_dump.cpp's per-board #if. blackbox.cpp implements
// the matching path; the enable-default below keys off this too.
#if defined(BOARD_LILYGO)
  #define BLACKBOX_BACKEND_SD       1
  #define BLACKBOX_BACKEND_NAME     "sd"
#elif defined(BOARD_WAVESHARE_S3) || defined(BOARD_LILYGO_T2CAN)
  #define BLACKBOX_BACKEND_LITTLEFS 1
  #define BLACKBOX_BACKEND_NAME     "littlefs"
#else
  #define BLACKBOX_BACKEND_RAM      1
  #define BLACKBOX_BACKEND_NAME     "ram"
#endif

// Default enable state: OFF on every backend. A fresh flash must boot with the
// recorder off so WiFi/dashboard always come up — the ~108 KB ring on a
// no-PSRAM S3 can starve the WiFi/web stack if grabbed at boot (#124). The user
// opts in from the dashboard, where the heap guard in blackbox_set_enabled()
// protects the allocation.
#define BLACKBOX_DEFAULT_ENABLED  false

enum BBTrigger : uint8_t {
    BB_TRIG_ABORT = 0,
    BB_TRIG_BUSOFF,
    BB_TRIG_MANUAL,
};

// Wire up state + storage backend and make the PSRAM/size decision. Does NOT
// allocate the ring — that happens lazily, heap-guarded, on enable (#124), so
// boot never starves the WiFi/web stack. Pass the shared state + mux so marks
// can inject through the event-core and flushes can snapshot toggles.
void blackbox_init(FSDState* state, portMUX_TYPE* state_mux);

// Record one RX frame into the ring. Cheap (id filter + memcpy); no-op when
// disabled or when the id isn't a key diagnostic id (see fsd_blackbox_filter.h).
void blackbox_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms);

// Record one injected TX frame into the ring — same id filter and ring path as
// blackbox_record, tagged TX so the capture distinguishes our frames from the
// bus. Called centrally from send_on_bus() so every injection (0x3EE / 0x3FD /
// the 0x370 nag echo) is captured without touching each call site.
void blackbox_record_tx(CanBusId bus, const CanFrame& frame, uint32_t now_ms);

// Note the current das_ap_state for the mini-timeline (call once per frame).
void blackbox_note_ap_state(uint8_t ap_state, uint32_t now_ms);

// Arm a capture from an abort/bus-off detected on the CAN path. `snap` is a
// snapshot of FSDState at trigger time (toggles, hw, evt_last_*). Ignored when
// disabled or a capture is already in flight.
void blackbox_arm(BBTrigger trig, const FSDState* snap, uint32_t now_ms);

// Inject a bus-off / manual mark through the event-core (applies the cooldown)
// and arm a capture if it fires. Used from the loop (bus-off) and dashboard.
void blackbox_busoff(uint32_t now_ms);
void blackbox_mark(uint32_t now_ms);

// Post-roll countdown + flush. Call once per loop().
void blackbox_tick(uint32_t now_ms);

// Enable/disable + persist; off stops recording and clears any armed capture.
void blackbox_set_enabled(bool enabled);
bool blackbox_is_enabled();

// Dashboard helpers.
String blackbox_status_json();              // {"enabled":..,"backend":..,...}
String blackbox_list_json();                // [{"name":..,"summary":{...}},..]
// Download: report the byte size of an event's .log/.json (false if missing),
// then stream just the body to the client. The caller owns the HTTP headers.
bool   blackbox_file_size(const char* name, bool json, size_t* size_out);
void   blackbox_stream_body(WiFiClient& client, const char* name, bool json);
bool   blackbox_delete(const char* name);
void   blackbox_delete_all();
