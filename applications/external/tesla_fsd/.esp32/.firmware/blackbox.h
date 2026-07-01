#pragma once
/*
 * blackbox.h — black-box incident recorder (ESP32, #124).
 *
 * A RAM ring buffer continuously records raw CAN frames (all ids, full rate,
 * all buses). When the shared event-core (fsd_events.h) reports an abort, a
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
 * Default ON; the enable toggle is persisted in NVS. Single-threaded: every
 * entry point is called from the Arduino loop()/CAN path.
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

// Default enable state. Persistent backends (LittleFS / SD) stream straight to
// file and survive a power cycle → default ON. The volatile RAM backend keeps a
// frozen copy in heap and loses events on reboot → default OFF (opt-in).
#if defined(BLACKBOX_BACKEND_RAM)
  #define BLACKBOX_DEFAULT_ENABLED  false
#else
  #define BLACKBOX_DEFAULT_ENABLED  true
#endif

enum BBTrigger : uint8_t {
    BB_TRIG_ABORT = 0,
    BB_TRIG_BUSOFF,
    BB_TRIG_MANUAL,
};

// Allocate the ring (PSRAM-aware) and mount the storage backend. Pass the
// shared state + critical-section mux so marks can inject through the
// event-core and flushes can snapshot toggles.
void blackbox_init(FSDState* state, portMUX_TYPE* state_mux);

// Record one RX frame into the ring. Cheap (a memcpy); no-op when disabled.
void blackbox_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms);

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
