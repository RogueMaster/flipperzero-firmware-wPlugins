/*
 * NFC Canary - shared types.
 *
 * Detects readers interrogating the device over 13.56 MHz NFC and alerts the
 * wearer. Vibration is the primary channel; this lives in a pocket, where a
 * beep is muffled and a screen is unseen.
 */
#pragma once

#include <furi.h>
#include <furi_hal_nfc.h>
#include <nfc/protocols/nfc_protocol.h>
#include <storage/storage.h> /* APP_DATA_PATH */
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>

#define TAG "NfcCanary"

#define SETTINGS_PATH  APP_DATA_PATH("settings.bin")
#define EVENTLOG_PATH  APP_DATA_PATH("events.csv")
/* saved_struct magic/version are uint8_t -- keep both in range. */
#define SETTINGS_MAGIC 0x4E
#define SETTINGS_VER   1

/* EFD is a hardware bit; polling it is cheap. 20ms keeps latency
 * imperceptible while leaving the CPU mostly idle. */
#define POLL_INTERVAL_MS 20

/* Readers do NOT emit a steady carrier. Phones in reader mode pulse the field
 * in bursts while polling, and terminals gap theirs between anticollision
 * attempts. A symmetric debounce is the wrong filter: the bit never holds
 * still long enough, so a real reader reads as noise. (Observed directly --
 * raw edges climbed while detections stayed at 0.)
 *
 * Asymmetric latch instead: trigger on the first rising edge, and stay
 * latched until the field has been continuously absent for the hold-off, so a
 * burst train collapses into one detection. */
#define FIELD_HOLDOFF_MS   750
#define FIELD_MIN_PULSE_MS 20

/* Info -> Warn promotion. Below this a carrier is a passing terminal, not an
 * interrogation of us. */
#define TIER_WARN_MS 300

#define SPEAKER_TIMEOUT 1000

#define EVENT_LOG_MAX   200 /* ring buffer; ~200 fits comfortably in RAM */
#define HISTORY_SLOTS   64 /* history strip columns */
#define HISTORY_SLOT_MS 5000 /* 64 * 5s = ~5 min of history */

/* ---------- threat tiers ---------- */

typedef enum {
    TierNone = 0,
    TierInfo, /* brief carrier -- logged, silent */
    TierWarn, /* sustained carrier -- probably aimed at us */
    TierAlarm, /* reader addressed us / issued a command */
} ThreatTier;

/* ---------- alert patterns ---------- */

typedef enum {
    PatternOff = 0,
    PatternSingle,
    PatternDouble,
    PatternSos,
    PatternPulse,
    PatternContinuous,
    PatternCount,
} AlertPattern;

typedef enum {
    ToneLow = 0, /* 1kHz */
    ToneMid, /* 2kHz  */
    ToneHigh, /* 3kHz  */
    ToneVeryHigh, /* 4kHz  */
    ToneCount,
} AlertTone;

typedef enum {
    ModeSentinel = 0, /* raw EFD: any carrier, any protocol, fully passive */
    ModeDecoy, /* listener: protocol + command bytes, but we respond */
    ModeCount,
} DetectMode;

/* ---------- settings (persisted) ---------- */

typedef struct {
    uint8_t mode; /* DetectMode */
    uint8_t vibro_pattern; /* AlertPattern */
    uint8_t sound_pattern; /* AlertPattern */
    uint8_t tone; /* AlertTone */
    uint8_t volume; /* 0-10 */
    uint8_t min_tier_vibro; /* lowest tier that vibrates */
    uint8_t min_tier_sound; /* lowest tier that beeps */
    bool silent; /* master: suppress sound, keep vibro+LED */
    bool force_vibro; /* override system vibro setting */
    bool led_enabled;
    bool log_enabled;
    bool seen_intro; /* first-run splash shown */
} AlerterSettings;

/* ---------- event log ---------- */

typedef struct {
    uint32_t timestamp; /* unix-ish seconds from RTC */
    uint16_t duration_ms; /* how long the field persisted (capped) */
    uint8_t tier; /* ThreatTier */
    uint8_t mode; /* DetectMode active at capture */
    uint8_t protocol; /* NfcProtocol, or 0xFF if unknown */
    uint8_t cmd; /* first command byte, 0 if none */
    uint8_t cmd_len;
    uint8_t reserved;
} AlertEvent;

/* ---------- runtime state ---------- */

typedef struct {
    FuriMutex* mutex;

    bool field_present; /* latched detection state */
    bool raw_field; /* undebounced EFD bit (diagnostics) */
    bool nfc_ok;
    uint32_t detections;
    uint32_t raw_edges;

    ThreatTier current_tier;
    uint32_t field_start_tick; /* when the current exposure began */

    /* session stats */
    uint32_t session_start;
    uint32_t count_info, count_warn, count_alarm;
    uint32_t first_event, last_event;
    uint32_t longest_ms;

    /* last observed protocol info (decoy mode) */
    uint8_t last_protocol;
    uint8_t last_cmd;

    /* history strip: highest tier seen per time slot */
    uint8_t history[HISTORY_SLOTS];
    uint32_t history_base_tick;

    /* event ring buffer */
    AlertEvent events[EVENT_LOG_MAX];
    uint16_t event_count; /* total ever, may exceed capacity */
    uint16_t event_head; /* next write index */
} AlerterState;
