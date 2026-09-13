// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// microSD persistence (path /ext/apps_data/timeclock/).
//
//   badges.csv   -> registered badges
//   punches.csv  -> punch history (date,time,name,uid,type)
//   config.txt   -> configuration (auto mode, PIN hash/salt, ...)
//   export.json  -> JSON export of the history (generated on demand)
//
// The data model lives here because storage is what serializes/deserializes it.
// =============================================================================

#include <furi.h>
#include <storage/storage.h>

// ---- Limits ----------------------------------------------------------------
#define TC_MAX_BADGES  64
#define TC_NAME_MAX    33 // 32 chars + terminator
#define TC_UID_STR_MAX 41 // up to 20 bytes in hex + terminator
#define TC_TECH_MAX    6 // "NFC" / "RFID" / "iBTN"
#define TC_DT_MAX      20 // "YYYY-MM-DD HH:MM"

// Release date of this build. If the Flipper's clock reads earlier than this,
// it was never set (or reset after a dead battery) and every timestamp this
// app writes would be wrong, so tc_date_is_valid() flags it for a warning.
#define TC_RELEASE_DATE "2026-09-13"

// ---- Event types -----------------------------------------------------------
typedef enum {
    TcEventNone = 0,
    TcEventIn = 1, // clock in
    TcEventOut = 2, // clock out
} TcEventType;

// ---- Badge model -----------------------------------------------------------
typedef struct {
    char uid[TC_UID_STR_MAX];
    char name[TC_NAME_MAX];
    char tech[TC_TECH_MAX];
    char created[TC_DT_MAX]; // registration date
    char last_used[TC_DT_MAX]; // last use
    TcEventType last_event; // last recorded punch
} Badge;

// ---- Persistent configuration ----------------------------------------------
typedef struct {
    bool auto_mode; // true = automatic IN/OUT
    bool sound_enabled; // true = play distinct IN/OUT sounds on punch
    bool vibro_enabled; // true = vibrate on punch (1 pulse IN, 2 pulses OUT)
    bool led_enabled; // true = blink LED on punch (green IN, blue OUT)
    bool pin_enabled; // true = protected mode active
    uint32_t pin_hash; // PIN hash (never stored in clear text)
    uint32_t pin_salt; // random salt used for the hash
    uint32_t attempts; // consecutive wrong PIN attempts
    bool onboarded; // first-launch onboarding already shown
    uint32_t language; // UI language index (see TcLang)
    uint32_t daily_target; // expected minutes worked per day (0 = off/no target)
} TcConfig;

// ---- File paths ------------------------------------------------------------
#define TC_DIR_PATH     EXT_PATH("apps_data/timeclock")
#define TC_BADGES_PATH  TC_DIR_PATH "/badges.csv"
#define TC_HISTORY_PATH TC_DIR_PATH "/punches.csv"
#define TC_CONFIG_PATH  TC_DIR_PATH "/config.txt"
#define TC_EXPORT_PATH  TC_DIR_PATH "/export.json"

// ---- Initialization --------------------------------------------------------
// Create the data folder if missing. Call once at startup.
void tc_storage_init(void);

// ---- Current date/time (RTC) -----------------------------------------------
void tc_now_date(char* out, size_t out_size); // "YYYY-MM-DD"
void tc_now_time(char* out, size_t out_size); // "HH:MM"
void tc_now_datetime(char* out, size_t out_size); // "YYYY-MM-DD HH:MM"

// False if the Flipper's RTC date is before TC_RELEASE_DATE (clock never set
// or reset). Every date/time this app records depends on the RTC being right.
bool tc_date_is_valid(void);

// ---- Config ----------------------------------------------------------------
void tc_config_load(TcConfig* config);
void tc_config_save(const TcConfig* config);

// ---- Badges ----------------------------------------------------------------
// Load badges from badges.csv into out_badges (max TC_MAX_BADGES).
// Returns the number of badges loaded.
size_t tc_badges_load(Badge* out_badges, size_t max);
// Rewrite badges.csv entirely with the provided array.
bool tc_badges_save(const Badge* badges, size_t count);

// ---- History ---------------------------------------------------------------
// Append a row to punches.csv (writes the header if needed).
bool tc_history_append(
    const char* date,
    const char* time,
    const char* name,
    const char* uid,
    TcEventType type);

// Load the whole history, formatted, into "out" for display.
// If filter_uid != NULL only that badge's rows are shown.
// If today_only == true only today's date is shown.
void tc_history_read(FuriString* out, const char* filter_uid, bool today_only);

// Load history rows whose date is within [from_date, to_date] (inclusive,
// "YYYY-MM-DD"; NULL = unbounded on that side), optionally filtered by UID.
// Date strings compare chronologically, so string bounds work directly.
void tc_history_read_range(
    FuriString* out,
    const char* filter_uid,
    const char* from_date,
    const char* to_date);

// Compute minutes worked on the given date "YYYY-MM-DD" (sum of IN/OUT pairs)
// for the given badge (or all badges if filter_uid == NULL). Also fills
// first-in and last-out if the pointers are not NULL.
uint32_t tc_history_minutes_for_date(
    const char* date_filter,
    const char* filter_uid,
    char* first_in,
    size_t first_in_size,
    char* last_out,
    size_t last_out_size);

// Convenience wrapper: same as above for today's date.
uint32_t tc_history_today_minutes(
    const char* filter_uid,
    char* first_in,
    size_t first_in_size,
    char* last_out,
    size_t last_out_size);

// Minutes worked in the given month "YYYY-MM" for a badge (or all if NULL),
// summed per day (same pairing as the daily total). Call per collaborator to
// avoid mixing different people's IN/OUT.
uint32_t tc_history_month_minutes(const char* month_prefix, const char* filter_uid);

// Remove the most recent punch of the given badge from the history. On success
// *new_last receives the badge's resulting last event (TcEventNone if none is
// left). Returns true if a punch was removed.
bool tc_history_undo_last(const char* uid, TcEventType* new_last);

// Clear the history (keeps only the header).
bool tc_history_clear(void);

// Export the history to export.json. Returns true on success.
bool tc_history_export_json(void);

// Filename-safe timestamp "YYYY-MM-DD_HHMM".
void tc_now_stamp(char* out, size_t out_size);

// Copy punches.csv to a dated snapshot punches-YYYY-MM-DD.csv. On success,
// out_name receives the file name. Returns true on success.
bool tc_export_csv_dated(char* out_name, size_t out_size);

// Copy only the current month's rows to punches-YYYY-MM.csv. On success out_name
// receives the file name. Returns true if at least one row was written.
bool tc_export_month_csv(char* out_name, size_t out_size);

// Copy badges.csv and punches.csv into a timestamped backup under backup/.
// out receives a short label. Returns true if at least one file was copied.
bool tc_backup_all(char* out, size_t out_size);

// List backup timestamps (from backup/badges-<ts>.csv) into out[][24], newest
// first not guaranteed. Returns the number found (up to max).
size_t tc_backup_list(char out[][24], size_t max);

// Restore badges + punches from the backup with the given timestamp. Returns
// true if at least one file was restored.
bool tc_backup_restore(const char* stamp);
