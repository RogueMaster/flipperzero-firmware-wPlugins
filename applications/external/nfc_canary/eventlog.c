/*
 * Event log: ring buffer in RAM, best-effort CSV mirror on SD.
 *
 * The log records THAT an interrogation happened -- timestamp, tier, protocol,
 * and which command the reader issued. It deliberately does not record card
 * contents or any credential material; this is a defensive tool, and logging
 * what a reader extracts would make it something else entirely.
 */

#include "eventlog.h"
#include "alert.h"

#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>

void eventlog_add(AlerterState* state, const AlertEvent* ev) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    state->events[state->event_head] = *ev;
    state->event_head = (state->event_head + 1) % EVENT_LOG_MAX;
    if(state->event_count < EVENT_LOG_MAX) state->event_count++;

    switch(ev->tier) {
    case TierInfo:
        state->count_info++;
        break;
    case TierWarn:
        state->count_warn++;
        break;
    case TierAlarm:
        state->count_alarm++;
        break;
    default:
        break;
    }

    if(state->first_event == 0) state->first_event = ev->timestamp;
    state->last_event = ev->timestamp;
    if(ev->duration_ms > state->longest_ms) state->longest_ms = ev->duration_ms;

    furi_mutex_release(state->mutex);
}

bool eventlog_get_locked(const AlerterState* state, uint16_t index, AlertEvent* out) {
    if(index >= state->event_count) return false;
    /* index 0 = newest, walking backwards from the head */
    uint16_t slot = (state->event_head + EVENT_LOG_MAX - 1 - index) % EVENT_LOG_MAX;
    *out = state->events[slot];
    return true;
}

uint16_t eventlog_count_locked(const AlerterState* state) {
    return state->event_count;
}

bool eventlog_get(AlerterState* state, uint16_t index, AlertEvent* out) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    bool ok = eventlog_get_locked(state, index, out);
    furi_mutex_release(state->mutex);
    return ok;
}

uint16_t eventlog_count(AlerterState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    uint16_t n = state->event_count;
    furi_mutex_release(state->mutex);
    return n;
}

void eventlog_clear(AlerterState* state) {
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    state->event_count = 0;
    state->event_head = 0;
    state->count_info = state->count_warn = state->count_alarm = 0;
    state->first_event = state->last_event = 0;
    state->longest_ms = 0;
    memset(state->history, 0, sizeof(state->history));
    furi_mutex_release(state->mutex);
}

void eventlog_persist(const AlertEvent* ev) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, EVENTLOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        /* Write a header once, when the file is empty. */
        if(stream_size(stream) == 0) {
            stream_write_cstring(stream, "timestamp,tier,mode,protocol,cmd,duration_ms\n");
        }
        char line[128];
        int n = snprintf(
            line,
            sizeof(line),
            "%lu,%s,%s,%s,%02X,%u\n",
            (unsigned long)ev->timestamp,
            alert_tier_name((ThreatTier)ev->tier),
            ev->mode == ModeDecoy ? "Decoy" : "Sentinel",
            eventlog_protocol_name(ev->protocol),
            ev->cmd,
            (unsigned)ev->duration_ms);
        if(n > 0) stream_write(stream, (uint8_t*)line, (size_t)n);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

const char* eventlog_protocol_name(uint8_t protocol) {
    switch(protocol) {
    case NfcProtocolIso14443_3a:
        return "ISO14443-3A";
    case NfcProtocolIso14443_3b:
        return "ISO14443-3B";
    case NfcProtocolIso14443_4a:
        return "ISO14443-4A";
    case NfcProtocolIso14443_4b:
        return "ISO14443-4B";
    case NfcProtocolIso15693_3:
        return "ISO15693";
    case NfcProtocolFelica:
        return "FeliCa";
    case NfcProtocolMfUltralight:
        return "MF Ultralight";
    case NfcProtocolMfClassic:
        return "MF Classic";
    case NfcProtocolMfPlus:
        return "MF Plus";
    case NfcProtocolMfDesfire:
        return "MF DESFire";
    default:
        return "Unknown";
    }
}

/*
 * Reader command decode. These are the bytes a reader sends once it has
 * selected a card, and they say a lot about intent: an anticollision sweep is
 * ordinary terminal behavior, but a Classic auth against a specific sector is
 * someone trying to read credentials.
 */
const char* eventlog_cmd_name(uint8_t cmd) {
    switch(cmd) {
    /* ISO14443-3A anticollision / selection */
    case 0x26:
        return "REQA (poll)";
    case 0x52:
        return "WUPA (wake)";
    case 0x93:
        return "SELECT cascade 1";
    case 0x95:
        return "SELECT cascade 2";
    case 0x97:
        return "SELECT cascade 3";
    case 0x50:
        return "HALT";
    case 0xE0:
        return "RATS (ISO-4 setup)";

    /* MIFARE Classic -- the interesting ones */
    case 0x60:
        return "Auth key A";
    case 0x61:
        return "Auth key B";
    case 0x30:
        return "Read block";
    case 0xA0:
        return "Write block";
    case 0xC1:
        return "Increment";
    case 0xC0:
        return "Decrement";
    case 0xB0:
        return "Transfer";

    /* Ultralight / NTAG */
    case 0x3A:
        return "Fast read";
    case 0xA2:
        return "Write page";
    case 0x1B:
        return "PWD_AUTH";
    case 0x3C:
        return "Read signature";

    /* DESFire */
    case 0x5A:
        return "Select application";
    case 0xAF:
        return "Additional frame";
    case 0x1A:
        return "Auth (AES/ISO)";
    case 0x0A:
        return "Auth (legacy)";
    case 0x6A:
        return "List applications";

    /* ISO15693 */
    case 0x01:
        return "Inventory";
    case 0x20:
        return "Read single block";
    case 0x21:
        return "Write single block";

    default:
        return "Unknown cmd";
    }
}
