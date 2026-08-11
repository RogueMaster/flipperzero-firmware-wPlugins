#include "meshcore_logger.h"

#include <datetime/datetime.h>
#include <furi_hal_rtc.h>

#include "../messenger/meshcore_contacts.h"
#include "meshcore_csv.h"
#include "meshcore_events.h"
#include "meshcore_ping.h"
#include "meshcore_rxlog.h"
#include "meshcore_telemetry.h"

#define MESHCORE_LOG_ROOT EXT_PATH("apps_data/meshcore_cfg")
#define MESHCORE_LOG_DIR MESHCORE_LOG_ROOT "/logs"

/* Worst case is an rx_log row: the raw packet can be the whole companion
 * payload minus the 3-byte push header (up to MC_MAX_PAYLOAD-3 = 252 bytes =
 * 504 hex characters), plus the timestamp, signal fields and node tags — still
 * comfortably under this. */
#define MESHCORE_LOG_LINE_MAX 640u
/* LoRa packet rates are low, so a shallow queue is plenty; it exists to keep
 * SD latency off the UART thread, not to buffer a flood. */
#define MESHCORE_LOG_QUEUE_DEPTH 8u
#define MESHCORE_LOG_WRITER_STACK 2048u
/* Sized against the measured worst frame rather than guessed: an mc_event_t
 * (252 bytes) plus a command payload (255) plus one MeshCoreLogLine (644) plus
 * the small formatting buffers, so a little over 1200 with snprintf on top.
 *
 * This started at 2048 and the thread died silently, taking the app with it.
 * Two things were wrong and both are fixed: the contact list was on the stack
 * (3664 bytes on its own — it now lives in the logger struct), and every row
 * was built in one buffer and copied into a second. The headroom below is
 * deliberate, and meshcore_logger_stack_report() will say if it is ever wrong
 * again instead of letting it crash. */
#define MESHCORE_LOG_POLLER_STACK 3072u
/* Below this much free stack, say so in the log rather than wait for the fault. */
#define MESHCORE_LOG_STACK_WARN 512u

/* meshlog.py's defaults, so a Flipper session and a phone session sample at the
 * same rate and their rows line up on a common time axis. */
#define MESHCORE_LOG_STATS_INTERVAL_MS 60000u
#define MESHCORE_LOG_PING_INTERVAL_MS 15000u
/* meshlog.py waits `interval - 1`, floored at two seconds. Same here, fixed:
 * the interval is not user-settable yet. */
#define MESHCORE_LOG_PING_TIMEOUT_MS 14000u
/* How often the poller wakes to check its two timers. Short enough that a stop
 * is not noticeably delayed, long enough not to matter. */
#define MESHCORE_LOG_POLLER_TICK_MS 250u

#define MESHCORE_RX_LOG_HEADER "ts,snr,rssi,lat,lon,acc,raw,node,role,hw"

/* Not in meshcore_c: it has no builder for GET_BATT_AND_STORAGE, only the
 * decoder for the reply. The code is the firmware's (MyMesh.cpp), and the
 * emulator answers the same one, so it is not a guess — but it is built here
 * rather than by patching the vendored library. */
#define MESHCORE_CMD_GET_BATT_AND_STORAGE 20u

/* Which file a queued row belongs to. Routing at the writer rather than at the
 * producer keeps every file behind the same single-writer discipline. */
typedef enum {
    MeshCoreLogFileRx,
    MeshCoreLogFileEvents,
    MeshCoreLogFileTelemetry,
    MeshCoreLogFilePing,
} MeshCoreLogFile;

typedef struct {
    MeshCoreLogFile target;
    char text[MESHCORE_LOG_LINE_MAX];
} MeshCoreLogLine;

struct MeshCoreLogger {
    MeshCoreLog* log;
    Storage* storage;

    MeshCoreLogNode node;

    char session_path[96];
    MeshCoreCsv* rx_log;
    MeshCoreCsv* events;
    MeshCoreCsv* telemetry;
    MeshCoreCsv* ping_log;

    /* Rows are handed to a writer thread rather than written where they are
     * produced: a synced SD write can take tens of milliseconds, and the
     * session worker that produces them is the same thread draining the UART.
     * Stalling it there would overrun the receive buffer. */
    FuriMessageQueue* queue;
    FuriThread* writer;
    /* Asks the node for numbers nobody pushes: battery, stats, and the ping
     * round trip. Separate from the writer because it blocks on the link, and
     * separate from the session worker because it blocks at all. */
    FuriThread* poller;
    /* Two stop flags, not one. `stop` releases the producers (poller and, by
     * proxy, the session); `writer_stop` releases the consumer. They are
     * distinct so the writer keeps draining until BOTH producers have been
     * joined -- otherwise the writer, keyed off the same flag, exits within one
     * ~250ms poll while the poller is still finishing a blocking request, and
     * every row emitted during teardown is queued to a thread that has already
     * returned. That silently broke the "clean stop loses nothing" contract. */
    volatile bool stop;
    volatile bool writer_stop;

    bool running;
    const char* error;

    MeshCorePing ping;
    /* Touched by both the poller (arming) and the session worker (confirming),
     * so the single-slot handoff between them is taken under this. */
    FuriMutex* ping_mutex;
    /* Here rather than on the poller's stack, where it belongs to nobody and
     * costs 3664 bytes -- a thread stack that size would be larger than the
     * app's own. Only the poller touches it, one refresh at a time. */
    MeshCoreContacts contacts;
    volatile uint32_t ping_ok;
    volatile uint32_t ping_sent;

    volatile uint32_t rx_count;
    volatile uint32_t dropped;
    volatile uint32_t marks;
    volatile bool have_rx;
    volatile int8_t last_snr_q4;
    volatile int8_t last_rssi;
};

static void meshcore_logger_emit(MeshCoreLogger* logger, MeshCoreLogFile target, MeshCoreLogLine* line);
static uint32_t meshcore_logger_ticks_to_ms(uint32_t ticks);

/* ---- small formatting helpers ---- */

static const char* meshcore_logger_role_name(MeshCoreLogRole role) {
    switch(role) {
    case MeshCoreLogRoleCompanion:
        return "companion";
    case MeshCoreLogRoleRepeater:
        return "repeater";
    default:
        return "unknown";
    }
}

static const char* meshcore_logger_hw_name(MeshCoreLogHw hw) {
    switch(hw) {
    case MeshCoreLogHwT114:
        return "t114";
    case MeshCoreLogHwV4:
        return "v4";
    default:
        return "unknown";
    }
}

/* Degrees x 1e6 to a plain decimal. Done in integers so a value round-trips
 * exactly; the sign is handled separately because -0.5 has a zero whole part. */
static void meshcore_logger_format_coord(int32_t value, char* out, size_t cap) {
    const char* sign = (value < 0) ? "-" : "";
    uint32_t magnitude = (uint32_t)(value < 0 ? -(int64_t)value : (int64_t)value);
    snprintf(
        out,
        cap,
        "%s%lu.%06lu",
        sign,
        (unsigned long)(magnitude / 1000000u),
        (unsigned long)(magnitude % 1000000u));
}

/* ISO-8601 without a zone, from the Flipper RTC. Single place to change if the
 * pipeline expects something else. */
static void meshcore_logger_timestamp(char* out, size_t cap) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    snprintf(
        out,
        cap,
        "%04u-%02u-%02uT%02u:%02u:%02u",
        (unsigned)now.year,
        (unsigned)now.month,
        (unsigned)now.day,
        (unsigned)now.hour,
        (unsigned)now.minute,
        (unsigned)now.second);
}

/* A node name with a comma in it would shift every later column. */
static void meshcore_logger_sanitise(const char* in, char* out, size_t cap) {
    size_t i = 0;
    for(; in[i] != '\0' && i + 1 < cap; i++) {
        char c = in[i];
        out[i] = (c == ',' || c == '"' || c == '\n' || c == '\r') ? '_' : c;
    }
    out[i] = '\0';
}

/* ---- writer thread ---- */

static MeshCoreCsv* meshcore_logger_file(MeshCoreLogger* logger, MeshCoreLogFile target) {
    switch(target) {
    case MeshCoreLogFileRx:
        return logger->rx_log;
    case MeshCoreLogFileEvents:
        return logger->events;
    case MeshCoreLogFileTelemetry:
        return logger->telemetry;
    case MeshCoreLogFilePing:
        return logger->ping_log;
    default:
        return NULL;
    }
}

static int32_t meshcore_logger_writer(void* context) {
    MeshCoreLogger* logger = context;
    MeshCoreLogLine line;

    /* writer_stop, not stop: the writer must outlive both producers so the rows
     * they emit while shutting down still get written. See the flag comment. */
    while(!logger->writer_stop) {
        if(furi_message_queue_get(logger->queue, &line, furi_ms_to_ticks(250)) != FuriStatusOk) {
            continue;
        }
        MeshCoreCsv* csv = meshcore_logger_file(logger, line.target);
        if(csv) meshcore_csv_write(csv, line.text);
    }

    /* Drain whatever is still queued so a clean stop loses nothing. */
    while(furi_message_queue_get(logger->queue, &line, 0) == FuriStatusOk) {
        MeshCoreCsv* csv = meshcore_logger_file(logger, line.target);
        if(csv) meshcore_csv_write(csv, line.text);
    }

    return 0;
}

/* Append the node tags to a row the caller has already built, and hand it to
 * the writer. Every row in every file ends the same way, so this is the only
 * place that knows about them.
 *
 * The caller owns the buffer and it is appended to in place. Taking a string
 * and copying it into a second MeshCoreLogLine here would put two 644-byte
 * buffers in one stack frame, which is most of a thread stack — and the poller
 * thread died of exactly that. */
static void meshcore_logger_emit(MeshCoreLogger* logger, MeshCoreLogFile target, MeshCoreLogLine* line) {
    char name[24];
    meshcore_logger_sanitise(logger->node.name, name, sizeof(name));

    size_t used = strlen(line->text);
    if(used < sizeof(line->text)) {
        snprintf(
            line->text + used,
            sizeof(line->text) - used,
            ",%s,%s,%s",
            name,
            meshcore_logger_role_name(logger->node.role),
            meshcore_logger_hw_name(logger->node.hw));
    }

    line->target = target;

    if(furi_message_queue_put(logger->queue, line, 0) != FuriStatusOk) {
        /* Counted rather than blocked on: losing a row beats overrunning the
         * UART and corrupting the ones that follow. emit() runs on three
         * threads (poller, session worker, GUI mark), so the increment is
         * atomic — a plain ++ would lose drops to a lost-update race, and this
         * counter's whole job is to be trusted when it is non-zero. Relaxed is
         * enough: readers only ever display it. */
        __atomic_add_fetch(&logger->dropped, 1, __ATOMIC_RELAXED);
    }
}

/* The position every row carries, taken from the node — the Flipper has no GPS
 * of its own. Empty strings when the node has never been given a fix. */
static void meshcore_logger_position(MeshCoreLogger* logger, char* lat, size_t lat_cap, char* lon, size_t lon_cap) {
    lat[0] = '\0';
    lon[0] = '\0';
    if(!logger->node.has_position) return;

    meshcore_logger_format_coord(logger->node.lat, lat, lat_cap);
    meshcore_logger_format_coord(logger->node.lon, lon, lon_cap);
}

/* ---- event sink ---- */

/* Runs on the session worker thread. Builds the row and hands it off; no file
 * I/O here, by design. */
static void meshcore_logger_on_event(
    const mc_event_t* event,
    const uint8_t* payload,
    size_t len,
    void* context) {
    MeshCoreLogger* logger = context;

    /* Roomy enough for the widest year the compiler will assume. */
    char ts[32];
    char lat[16];
    char lon[16];
    /* One row buffer for the whole function, reused by whichever branch runs.
     * This thread's stack is 2 KB and the buffer is 644 bytes of it. */
    MeshCoreLogLine line;

    meshcore_logger_timestamp(ts, sizeof(ts));
    meshcore_logger_position(logger, lat, sizeof(lat), lon, sizeof(lon));

    if(event->code == MESHCORE_RXLOG_CODE) {
        MeshCoreRxLog rx;
        if(!meshcore_rxlog_parse(payload, len, &rx)) return;

        logger->rx_count++;
        logger->last_snr_q4 = rx.snr_q4;
        logger->last_rssi = rx.rssi;
        logger->have_rx = true;

        char snr[MESHCORE_SNR_LEN];
        meshcore_rxlog_format_snr(rx.snr_q4, snr, sizeof(snr));

        /* Hex the packet straight into the row. Sized for the largest raw a
         * 0x88 push can carry — the whole companion payload minus its 3-byte
         * header — so a big flood/path-heavy packet is logged verbatim, not
         * truncated. */
        char raw[2 * MC_MAX_PAYLOAD + 1];
        meshcore_hex_encode(rx.raw, rx.raw_len, raw, sizeof(raw));

        /* ts,snr,rssi,lat,lon,acc,raw — acc is always empty, the companion
         * protocol carries no position accuracy. */
        snprintf(
            line.text, sizeof(line.text), "%s,%s,%d,%s,%s,,%s", ts, snr, (int)rx.rssi, lat, lon, raw);
        meshcore_logger_emit(logger, MeshCoreLogFileRx, &line);
        return;
    }

    /* A heard advert. NEW_ADVERT carries the whole contact record; the plain
     * ADVERT push carries only the key, which is still enough to tell two
     * unknown neighbours apart in the log. */
    if(event->code == MC_PUSH_NEW_ADVERT || event->code == MC_PUSH_ADVERT) {
        char info[32];
        char raw[24];

        if(event->code == MC_PUSH_NEW_ADVERT) {
            meshcore_logger_sanitise(event->u.contact.adv_name, info, sizeof(info));
            meshcore_event_key_prefix(event->u.contact.public_key, 6, raw, sizeof(raw));
        } else {
            /* The bare push carries nothing but the key. */
            info[0] = '\0';
            meshcore_event_key_prefix(event->u.pubkey32, 6, raw, sizeof(raw));
        }

        meshcore_event_format(
            MeshCoreEventAdvert, ts, info, raw, lat, lon, line.text, sizeof(line.text));
        meshcore_logger_emit(logger, MeshCoreLogFileEvents, &line);
        return;
    }

    /* The ack that closes an outstanding ping. Matched here rather than in the
     * poller because this is the thread the push arrives on; the poller only
     * ever learns the outcome. */
    if(event->code == MC_PUSH_SEND_CONFIRMED) {
        uint32_t rtt = 0;
        bool matched = false;
        size_t index = 0;
        uint32_t seq = 0;
        uint32_t ack_code = 0;

        /* From the raw payload, not from event->u.send_confirmed: meshcore_c
         * wants eight bytes and leaves the union untouched when a node sends
         * the four-byte form, which reads back as ack code zero and matches
         * nothing. See meshcore_ping_parse_ack. */
        if(!meshcore_ping_parse_ack(payload, len, &ack_code)) return;

        /* Snapshot the fields the unmatched-ack diagnostic prints while the
         * lock is held; the poller mutates them under the same lock. */
        bool was_in_flight;
        uint32_t was_expecting;

        furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
        index = logger->ping.flight_index;
        seq = logger->ping.flight_seq;
        was_in_flight = logger->ping.in_flight;
        was_expecting = logger->ping.expected_ack;
        matched = meshcore_ping_confirm(&logger->ping, ack_code, furi_get_tick(), &rtt);
        furi_mutex_release(logger->ping_mutex);

        if(matched) {
            char target[MESHCORE_PING_NAME_LEN];
            meshcore_logger_sanitise(logger->ping.targets[index].name, target, sizeof(target));
            meshcore_ping_format(
                ts,
                target,
                seq,
                true,
                meshcore_logger_ticks_to_ms(rtt),
                lat,
                lon,
                line.text,
                sizeof(line.text));
            meshcore_logger_emit(logger, MeshCoreLogFilePing, &line);
            logger->ping_ok++;
        } else {
            /* An ack for something we are not timing. Worth saying: it means
             * either the round trip outlived its timeout, or the tag did not
             * match, and those are different problems with the same symptom of
             * a ping.csv full of misses. */
            meshcore_log_printf(
                logger->log,
                "ack %08lx unmatched (pending=%d want=%08lx)",
                (unsigned long)ack_code,
                (int)was_in_flight,
                (unsigned long)was_expecting);
        }
        return;
    }

    /* An incoming message. The mailbox belongs to the messenger; here it is
     * only evidence that a packet made the whole trip, which is exactly what a
     * link test wants to record. */
    if(event->code == MC_RESP_CONTACT_MSG_RECV || event->code == MC_RESP_CONTACT_MSG_RECV_V3 ||
       event->code == MC_RESP_CHANNEL_MSG_RECV || event->code == MC_RESP_CHANNEL_MSG_RECV_V3) {
        char info[64];

        /* Contact and channel messages land in different union members; a
         * channel message's text already carries "Name: body". */
        const bool channel = (event->code == MC_RESP_CHANNEL_MSG_RECV ||
                              event->code == MC_RESP_CHANNEL_MSG_RECV_V3);
        meshcore_logger_sanitise(
            channel ? event->u.channel_msg.text : event->u.contact_msg.text, info, sizeof(info));

        meshcore_event_format(
            MeshCoreEventMessage, ts, info, "", lat, lon, line.text, sizeof(line.text));
        meshcore_logger_emit(logger, MeshCoreLogFileEvents, &line);
        return;
    }
}

/* ---- poller thread ---- */

/* Ticks are milliseconds on this platform, but saying so in one place is
 * cheaper than being wrong everywhere if that ever changes. */
static uint32_t meshcore_logger_ticks_to_ms(uint32_t ticks) {
    uint32_t hz = furi_kernel_get_tick_frequency();
    if(hz == 0) return ticks;
    return (uint32_t)(((uint64_t)ticks * 1000u) / hz);
}

static bool meshcore_logger_stats(MeshCoreLogger* logger, uint8_t subtype, mc_event_t* event) {
    uint8_t payload[MC_MAX_PAYLOAD];
    size_t len = mc_cmd_get_stats(payload, sizeof(payload), subtype);
    if(len == 0) return false;

    if(!meshcore_session_request(
           logger->node.session, payload, len, MC_RESP_STATS, event, MESHCORE_LINK_TIMEOUT_MS)) {
        return false;
    }
    /* A node that answers the wrong subtype would silently fill the row with
     * another block's numbers. */
    return event->u.stats.subtype == subtype;
}

static void meshcore_logger_sample_telemetry(MeshCoreLogger* logger) {
    MeshCoreTelemetry telemetry;
    mc_event_t event;
    uint8_t payload[MC_MAX_PAYLOAD];

    memset(&telemetry, 0, sizeof(telemetry));

    /* Battery first, from the dedicated command. The CORE stats block carries
     * it too; that is the fallback below, because older firmware answers one
     * and not the other. */
    payload[0] = MESHCORE_CMD_GET_BATT_AND_STORAGE;
    if(meshcore_session_request(
           logger->node.session,
           payload,
           1,
           MC_RESP_BATTERY_VOLTAGE,
           &event,
           MESHCORE_LINK_TIMEOUT_MS)) {
        telemetry.have_battery = true;
        telemetry.battery_mv = event.u.battery_mv;
    }

    if(meshcore_logger_stats(logger, MC_STATS_CORE, &event)) {
        telemetry.have_core = true;
        telemetry.uptime_secs = event.u.stats.u.core.uptime_secs;
        telemetry.core_errors = event.u.stats.u.core.errors;
        telemetry.queue_len = event.u.stats.u.core.queue_len;
        if(!telemetry.have_battery) {
            telemetry.have_battery = true;
            telemetry.battery_mv = event.u.stats.u.core.battery_mv;
        }
    }

    if(meshcore_logger_stats(logger, MC_STATS_RADIO, &event)) {
        telemetry.have_radio = true;
        telemetry.noise_floor = event.u.stats.u.radio.noise_floor;
        telemetry.last_rssi = event.u.stats.u.radio.last_rssi;
        telemetry.last_snr_q4 = event.u.stats.u.radio.last_snr_q4;
        telemetry.tx_air_secs = event.u.stats.u.radio.tx_air_secs;
        telemetry.rx_air_secs = event.u.stats.u.radio.rx_air_secs;
    }

    if(meshcore_logger_stats(logger, MC_STATS_PACKETS, &event)) {
        telemetry.have_packets = true;
        telemetry.recv = event.u.stats.u.packets.recv;
        telemetry.sent = event.u.stats.u.packets.sent;
        telemetry.flood_tx = event.u.stats.u.packets.flood_tx;
        telemetry.direct_tx = event.u.stats.u.packets.direct_tx;
        telemetry.flood_rx = event.u.stats.u.packets.flood_rx;
        telemetry.direct_rx = event.u.stats.u.packets.direct_rx;
        telemetry.recv_errors = event.u.stats.u.packets.recv_errors;
        telemetry.has_recv_errors = event.u.stats.has_recv_errors != 0;
    }

    /* A row with every field empty says "the node stopped answering", which is
     * itself worth recording — so it is written rather than skipped. */
    char ts[32];
    char lat[16];
    char lon[16];
    MeshCoreLogLine line;

    meshcore_logger_timestamp(ts, sizeof(ts));
    meshcore_logger_position(logger, lat, sizeof(lat), lon, sizeof(lon));
    meshcore_telemetry_format(&telemetry, ts, lat, lon, line.text, sizeof(line.text));
    meshcore_logger_emit(logger, MeshCoreLogFileTelemetry, &line);
}

/* Ping needs public keys, and those only exist once the contact list has been
 * read. Re-read periodically: a node heard for the first time halfway through
 * a walk is exactly the one worth measuring. */
static void meshcore_logger_refresh_targets(MeshCoreLogger* logger) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;
    MeshCoreContacts* contacts = &logger->contacts;

    size_t len = mc_cmd_get_contacts(payload, sizeof(payload), 0);
    if(len == 0) return;

    meshcore_contacts_reset(contacts);
    if(!meshcore_session_request_stream(
           logger->node.session,
           payload,
           len,
           MC_RESP_END_OF_CONTACTS,
           meshcore_contacts_collect,
           contacts,
           &event,
           MESHCORE_LINK_TIMEOUT_MS)) {
        return;
    }

    meshcore_contacts_sort_by_last_seen(contacts);

    furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
    for(size_t i = 0; i < contacts->count; i++) {
        const MeshCoreContact* contact = &contacts->items[i];
        if(contact->name[0] == '\0') continue;
        /* add() refuses duplicates and a full table, so this converges on the
         * first few most recently heard and then stops changing. */
        meshcore_ping_add(&logger->ping, contact->name);
        meshcore_ping_resolve(&logger->ping, contact->name, contact->public_key);
    }
    furi_mutex_release(logger->ping_mutex);
}

/* One ping: a text message out, the ack tag from the reply recorded, and the
 * round trip closed later by the session worker when SEND_CONFIRMED arrives. */
static void meshcore_logger_ping_once(MeshCoreLogger* logger) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;
    char text[32];
    uint8_t pubkey[32];
    char name[MESHCORE_PING_NAME_LEN];
    size_t index = 0;
    uint32_t seq = 0;
    bool known = false;

    furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
    MeshCorePingTarget* target = meshcore_ping_next(&logger->ping);
    if(target != NULL) {
        index = (size_t)(target - logger->ping.targets);
        known = target->known;
        target->seq++;
        seq = target->seq;
        memcpy(pubkey, target->pubkey, sizeof(pubkey));
        snprintf(name, sizeof(name), "%s", target->name);
    }
    furi_mutex_release(logger->ping_mutex);

    if(target == NULL) return;

    char ts[32];
    char lat[16];
    char lon[16];
    char safe[MESHCORE_PING_NAME_LEN];
    MeshCoreLogLine line;

    meshcore_logger_timestamp(ts, sizeof(ts));
    meshcore_logger_position(logger, lat, sizeof(lat), lon, sizeof(lon));
    meshcore_logger_sanitise(name, safe, sizeof(safe));

    /* Not in the contact list yet: recorded as a miss rather than skipped,
     * because "could not be reached" is the measurement. */
    if(!known) {
        meshcore_ping_format(ts, safe, seq, false, 0, lat, lon, line.text, sizeof(line.text));
        meshcore_logger_emit(logger, MeshCoreLogFilePing, &line);
        return;
    }

    snprintf(text, sizeof(text), "ping %lu", (unsigned long)seq);
    size_t len = mc_cmd_send_txt_msg(
        payload, sizeof(payload), MC_TXT_PLAIN, 0, (uint32_t)furi_hal_rtc_get_timestamp(), pubkey, 6, text);
    if(len == 0) return;

    logger->ping_sent++;

    if(!meshcore_session_request(
           logger->node.session, payload, len, MC_RESP_SENT, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        /* The node would not even take it — no ack is coming, so close the
         * measurement now instead of waiting out the timeout. */
        meshcore_ping_format(ts, safe, seq, false, 0, lat, lon, line.text, sizeof(line.text));
        meshcore_logger_emit(logger, MeshCoreLogFilePing, &line);
        return;
    }

    furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
    meshcore_ping_started(
        &logger->ping, index, seq, event.u.msg_sent.expected_ack, furi_get_tick());
    furi_mutex_release(logger->ping_mutex);
}

/* The outstanding ping has waited long enough. Recorded as a miss — the row
 * that makes a loss percentage meaningful. */
static void meshcore_logger_ping_expire(MeshCoreLogger* logger) {
    size_t index;
    uint32_t seq;

    furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
    index = logger->ping.flight_index;
    seq = logger->ping.flight_seq;
    bool expired = meshcore_ping_timeout(&logger->ping);
    furi_mutex_release(logger->ping_mutex);

    if(!expired) return;

    char ts[32];
    char lat[16];
    char lon[16];
    char safe[MESHCORE_PING_NAME_LEN];
    MeshCoreLogLine line;

    meshcore_logger_timestamp(ts, sizeof(ts));
    meshcore_logger_position(logger, lat, sizeof(lat), lon, sizeof(lon));
    meshcore_logger_sanitise(logger->ping.targets[index].name, safe, sizeof(safe));
    meshcore_ping_format(ts, safe, seq, false, 0, lat, lon, line.text, sizeof(line.text));
    meshcore_logger_emit(logger, MeshCoreLogFilePing, &line);
}

/* A thread that overflows its stack does not report anything — it faults, and
 * the app disappears. Checking the low-water mark where the deepest calls have
 * already happened turns that into a line in the log. */
static void meshcore_logger_stack_report(MeshCoreLogger* logger) {
    uint32_t free_bytes = furi_thread_get_stack_space(furi_thread_get_current_id());
    if(free_bytes < MESHCORE_LOG_STACK_WARN) {
        meshcore_log_printf(
            logger->log, "logger: poller stack low, %lu free", (unsigned long)free_bytes);
    }
}

static int32_t meshcore_logger_poller(void* context) {
    MeshCoreLogger* logger = context;

    uint32_t next_stats = furi_get_tick();
    uint32_t next_ping = furi_get_tick() + furi_ms_to_ticks(MESHCORE_LOG_PING_INTERVAL_MS);
    uint32_t next_targets = furi_get_tick();

    while(!logger->stop) {
        furi_delay_ms(MESHCORE_LOG_POLLER_TICK_MS);
        if(logger->stop) break;

        uint32_t now = furi_get_tick();

        /* Signed comparison so the wrap of the tick counter does not park the
         * next sample four billion ticks into the future. */
        if((int32_t)(now - next_targets) >= 0) {
            next_targets = now + furi_ms_to_ticks(MESHCORE_LOG_STATS_INTERVAL_MS);
            meshcore_logger_refresh_targets(logger);
            if(logger->stop) break;
            now = furi_get_tick();
        }

        if((int32_t)(now - next_stats) >= 0) {
            next_stats = now + furi_ms_to_ticks(MESHCORE_LOG_STATS_INTERVAL_MS);
            meshcore_logger_sample_telemetry(logger);
            /* Right after the deepest call this thread makes. */
            meshcore_logger_stack_report(logger);
            if(logger->stop) break;
            now = furi_get_tick();
        }

        bool in_flight;
        uint32_t sent;
        furi_mutex_acquire(logger->ping_mutex, FuriWaitForever);
        in_flight = logger->ping.in_flight;
        sent = logger->ping.sent_ms;
        furi_mutex_release(logger->ping_mutex);

        if(in_flight) {
            if((int32_t)(now - sent) >= (int32_t)furi_ms_to_ticks(MESHCORE_LOG_PING_TIMEOUT_MS)) {
                meshcore_logger_ping_expire(logger);
            }
            continue; /* one in flight at a time, by design */
        }

        if((int32_t)(now - next_ping) >= 0) {
            next_ping = now + furi_ms_to_ticks(MESHCORE_LOG_PING_INTERVAL_MS);
            meshcore_logger_ping_once(logger);
        }
    }

    return 0;
}

/* ---- lifecycle ---- */

MeshCoreLogger* meshcore_logger_alloc(MeshCoreLog* log) {
    MeshCoreLogger* logger = malloc(sizeof(MeshCoreLogger));

    logger->log = log;
    logger->storage = furi_record_open(RECORD_STORAGE);

    memset(&logger->node, 0, sizeof(logger->node));
    logger->node.serial_id = FuriHalSerialIdUsart;
    logger->node.role = MeshCoreLogRoleCompanion; /* stage 1: assumed */
    logger->node.hw = MeshCoreLogHwUnknown;
    snprintf(logger->node.name, sizeof(logger->node.name), "node");

    logger->session_path[0] = '\0';
    logger->rx_log = NULL;
    logger->events = NULL;
    logger->telemetry = NULL;
    logger->ping_log = NULL;
    logger->queue = furi_message_queue_alloc(MESHCORE_LOG_QUEUE_DEPTH, sizeof(MeshCoreLogLine));
    logger->writer = NULL;
    logger->poller = NULL;
    logger->stop = false;
    logger->writer_stop = false;
    logger->running = false;
    logger->error = NULL;
    meshcore_ping_init(&logger->ping);
    logger->ping_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    meshcore_contacts_reset(&logger->contacts);
    logger->ping_ok = 0;
    logger->ping_sent = 0;
    logger->rx_count = 0;
    logger->dropped = 0;
    logger->marks = 0;
    logger->have_rx = false;
    logger->last_snr_q4 = 0;
    logger->last_rssi = 0;

    return logger;
}

void meshcore_logger_free(MeshCoreLogger* logger) {
    furi_assert(logger);

    meshcore_logger_stop(logger);
    furi_message_queue_free(logger->queue);
    furi_mutex_free(logger->ping_mutex);
    furi_record_close(RECORD_STORAGE);
    free(logger);
}

/* /ext/apps_data/meshcore_cfg/logs/<YYYYMMDD-HHMMSS>/ */
static bool meshcore_logger_make_session_dir(MeshCoreLogger* logger) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    /* Each level separately: storage_simply_mkdir does not create parents. */
    storage_simply_mkdir(logger->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(logger->storage, MESHCORE_LOG_ROOT);
    storage_simply_mkdir(logger->storage, MESHCORE_LOG_DIR);

    snprintf(
        logger->session_path,
        sizeof(logger->session_path),
        MESHCORE_LOG_DIR "/%04u%02u%02u-%02u%02u%02u",
        (unsigned)now.year,
        (unsigned)now.month,
        (unsigned)now.day,
        (unsigned)now.hour,
        (unsigned)now.minute,
        (unsigned)now.second);

    return storage_simply_mkdir(logger->storage, logger->session_path);
}

/* Ask the node who it is, so rows can be tagged and positions filled in. */
static void meshcore_logger_read_node(MeshCoreLogger* logger) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    size_t len = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);
    if(len == 0) return;

    if(!meshcore_session_request(
           logger->node.session, payload, len, MC_RESP_SELF_INFO, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        /* Not fatal: RX log pushes do not depend on the handshake, so we can
         * still record signal data from an unidentified node. */
        meshcore_log_printf(logger->log, "logger: no self-info, tagging as unknown");
        return;
    }

    if(event.u.self_info.name[0] != '\0') {
        /* A node name can be up to MC_MAX_TEXT; the tag column wants
         * something short, and a truncated name still identifies the node. */
        snprintf(logger->node.name, sizeof(logger->node.name), "%.23s", event.u.self_info.name);
    }

    /* A node that has never been given a position advertises 0,0. Treating
     * that as a real fix would put every walk off the coast of Africa. */
    if(event.u.self_info.adv_lat != 0 || event.u.self_info.adv_lon != 0) {
        logger->node.lat = event.u.self_info.adv_lat;
        logger->node.lon = event.u.self_info.adv_lon;
        logger->node.has_position = true;
    }

    /* Hardware detection proper is stage 4; DEVICE_INFO carries the model. */
    len = mc_cmd_device_query(payload, sizeof(payload), MESHCORE_LINK_PROTO_VER);
    if(len != 0 && meshcore_session_request(
                       logger->node.session,
                       payload,
                       len,
                       MC_RESP_DEVICE_INFO,
                       &event,
                       MESHCORE_LINK_TIMEOUT_MS)) {
        const char* model = event.u.device_info.model;
        if(strstr(model, "T114") || strstr(model, "t114")) {
            logger->node.hw = MeshCoreLogHwT114;
        } else if(strstr(model, "V4") || strstr(model, "v4")) {
            logger->node.hw = MeshCoreLogHwV4;
        }
    }
}

/* Closing a NULL handle is a no-op, so this is safe both as cleanup after a
 * partial open and as the normal shutdown path. */
static void meshcore_logger_close_files(MeshCoreLogger* logger) {
    meshcore_csv_close(logger->rx_log);
    meshcore_csv_close(logger->events);
    meshcore_csv_close(logger->telemetry);
    meshcore_csv_close(logger->ping_log);
    logger->rx_log = NULL;
    logger->events = NULL;
    logger->telemetry = NULL;
    logger->ping_log = NULL;
}

bool meshcore_logger_start(MeshCoreLogger* logger) {
    furi_assert(logger);
    if(logger->running) return true;

    logger->error = NULL;
    logger->rx_count = 0;
    logger->dropped = 0;
    logger->have_rx = false;

    if(!meshcore_logger_make_session_dir(logger)) {
        logger->error = "Cannot create the log folder.\nIs an SD card inserted?";
        return false;
    }

    /* All four open up front. A file created later, mid-walk, would be missing
     * exactly the rows from before someone noticed. */
    static const struct {
        const char* name;
        const char* header;
        size_t offset;
    } files[] = {
        {"rx_log.csv", MESHCORE_RX_LOG_HEADER, offsetof(MeshCoreLogger, rx_log)},
        {"events.csv", MESHCORE_EVENTS_HEADER, offsetof(MeshCoreLogger, events)},
        {"telemetry.csv", MESHCORE_TELEMETRY_HEADER, offsetof(MeshCoreLogger, telemetry)},
        {"ping.csv", MESHCORE_PING_HEADER, offsetof(MeshCoreLogger, ping_log)},
    };

    char path[128];
    for(size_t i = 0; i < COUNT_OF(files); i++) {
        snprintf(path, sizeof(path), "%s/%s", logger->session_path, files[i].name);
        MeshCoreCsv* csv = meshcore_csv_open(logger->storage, path, files[i].header);
        *(MeshCoreCsv**)((uint8_t*)logger + files[i].offset) = csv;
        if(csv == NULL) {
            meshcore_logger_close_files(logger);
            logger->error = "Cannot open the CSV files.\nIs the card write-protected?";
            return false;
        }
    }

    logger->node.session = meshcore_session_alloc(logger->log, logger->node.serial_id);
    meshcore_session_set_event_callback(logger->node.session, meshcore_logger_on_event, logger);

    if(!meshcore_session_start(logger->node.session)) {
        meshcore_session_free(logger->node.session);
        logger->node.session = NULL;
        meshcore_logger_close_files(logger);
        logger->error = "Cannot take the UART.\nAnother app is holding it.";
        return false;
    }

    logger->stop = false;
    logger->writer_stop = false;
    logger->writer = furi_thread_alloc_ex(
        "MeshCoreLogWriter", MESHCORE_LOG_WRITER_STACK, meshcore_logger_writer, logger);
    furi_thread_start(logger->writer);

    logger->running = true;

    /* After the writer is up, so anything the handshake shakes loose is
     * recorded rather than dropped. */
    meshcore_logger_read_node(logger);

    /* Last: it asks the node questions, and the handshake above should have
     * the link to itself while it establishes who we are talking to. */
    logger->poller = furi_thread_alloc_ex(
        "MeshCoreLogPoll", MESHCORE_LOG_POLLER_STACK, meshcore_logger_poller, logger);
    furi_thread_start(logger->poller);

    meshcore_log_printf(logger->log, "logger: %s", logger->session_path);
    return true;
}

void meshcore_logger_stop(MeshCoreLogger* logger) {
    furi_assert(logger);
    if(!logger->running) return;

    /* The poller goes first and is joined before the session is torn down: it
     * makes blocking calls into the session, and freeing that underneath it
     * would leave it holding a dead link. */
    logger->stop = true;
    if(logger->poller) {
        furi_thread_join(logger->poller);
        furi_thread_free(logger->poller);
        logger->poller = NULL;
    }

    /* Session next: it feeds the queue, so it has to stop producing before the
     * writer stops consuming. Its worker delivers any buffered frame to
     * on_event -> emit as it joins, so the writer must still be running here. */
    if(logger->node.session) {
        meshcore_session_free(logger->node.session);
        logger->node.session = NULL;
    }

    /* Only now, with both producers stopped and everything they emitted already
     * in the queue, tell the writer to finish. Its post-loop drain then catches
     * every teardown row. */
    logger->writer_stop = true;
    furi_thread_join(logger->writer);
    furi_thread_free(logger->writer);
    logger->writer = NULL;

    meshcore_logger_close_files(logger);

    logger->running = false;
}

bool meshcore_logger_is_running(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->running;
}

const char* meshcore_logger_error(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->error;
}

const char* meshcore_logger_session_path(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->session_path;
}

const MeshCoreLogNode* meshcore_logger_node(MeshCoreLogger* logger) {
    furi_assert(logger);
    return &logger->node;
}

uint32_t meshcore_logger_rx_count(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->rx_count;
}

uint32_t meshcore_logger_dropped(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->dropped;
}

bool meshcore_logger_last_rx(MeshCoreLogger* logger, int8_t* snr_q4, int8_t* rssi) {
    furi_assert(logger);
    if(!logger->have_rx) return false;
    if(snr_q4) *snr_q4 = logger->last_snr_q4;
    if(rssi) *rssi = logger->last_rssi;
    return true;
}

void meshcore_logger_mark(MeshCoreLogger* logger) {
    furi_assert(logger);
    if(!logger->running) return;

    char ts[32];
    char lat[16];
    char lon[16];
    char info[16];
    MeshCoreLogLine line;

    uint32_t number = ++logger->marks;

    meshcore_logger_timestamp(ts, sizeof(ts));
    meshcore_logger_position(logger, lat, sizeof(lat), lon, sizeof(lon));
    snprintf(info, sizeof(info), "%lu", (unsigned long)number);

    meshcore_event_format(MeshCoreEventMark, ts, info, "", lat, lon, line.text, sizeof(line.text));
    meshcore_logger_emit(logger, MeshCoreLogFileEvents, &line);
}

uint32_t meshcore_logger_marks(MeshCoreLogger* logger) {
    furi_assert(logger);
    return logger->marks;
}

void meshcore_logger_ping_stats(MeshCoreLogger* logger, uint32_t* sent, uint32_t* ok) {
    furi_assert(logger);
    if(sent) *sent = logger->ping_sent;
    if(ok) *ok = logger->ping_ok;
}
