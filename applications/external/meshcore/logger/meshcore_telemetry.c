#include "meshcore_telemetry.h"

#include <stdio.h>

/* meshlog.py wrote JSON here, quoted by csv.writer. We have no JSON encoder
 * and no quoting, so these carry the same information as `k=v;k=v` instead.
 * Semicolons, not commas — a comma would shift every later column.
 *
 * They are not decoration: the named columns drop uptime, queue depth, air
 * time and the flood/direct split, and those are exactly the numbers you want
 * when a node behaves oddly and the walk is already over. */
static size_t meshcore_telemetry_raw_bat(const MeshCoreTelemetry* t, char* out, size_t cap) {
    if(!t->have_battery) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)snprintf(out, cap, "mv=%u", (unsigned)t->battery_mv);
}

static size_t meshcore_telemetry_raw_radio(const MeshCoreTelemetry* t, char* out, size_t cap) {
    if(!t->have_radio) {
        out[0] = '\0';
        return 0;
    }
    /* SNR is carried as quarter-dB; printed here in whole quarters to keep this
     * file free of float formatting, which costs several kilobytes on the FAP.
     * The rx_log column is where a human-readable SNR belongs. */
    return (size_t)snprintf(
        out,
        cap,
        "nf=%d;rssi=%d;snr_q4=%d;tx_air=%lu;rx_air=%lu",
        (int)t->noise_floor,
        (int)t->last_rssi,
        (int)t->last_snr_q4,
        (unsigned long)t->tx_air_secs,
        (unsigned long)t->rx_air_secs);
}

/* uptime, queue depth and the CORE error counter. The comment above promised
 * these were preserved somewhere; before this they were collected once a minute
 * and thrown away, so the column that keeps that promise now exists. */
static size_t meshcore_telemetry_raw_core(const MeshCoreTelemetry* t, char* out, size_t cap) {
    if(!t->have_core) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)snprintf(
        out,
        cap,
        "up=%lu;q=%u;cerr=%u",
        (unsigned long)t->uptime_secs,
        (unsigned)t->queue_len,
        (unsigned)t->core_errors);
}

static size_t meshcore_telemetry_raw_pkts(const MeshCoreTelemetry* t, char* out, size_t cap) {
    if(!t->have_packets) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)snprintf(
        out,
        cap,
        "recv=%lu;sent=%lu;ftx=%lu;dtx=%lu;frx=%lu;drx=%lu",
        (unsigned long)t->recv,
        (unsigned long)t->sent,
        (unsigned long)t->flood_tx,
        (unsigned long)t->direct_tx,
        (unsigned long)t->flood_rx,
        (unsigned long)t->direct_rx);
}

/* Millivolts to volts with three decimals, in integers: the value comes off the
 * wire as an integer and should round-trip as one. */
static void meshcore_telemetry_volts(uint16_t mv, char* out, size_t cap) {
    snprintf(out, cap, "%u.%03u", (unsigned)(mv / 1000u), (unsigned)(mv % 1000u));
}

size_t meshcore_telemetry_format(
    const MeshCoreTelemetry* t,
    const char* ts,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap) {
    char raw_bat[32];
    char raw_radio[72];
    char raw_pkts[96];
    char raw_core[40];
    char batt[8] = "";
    char voltage[12] = "";
    char noise[8] = "";
    char rx_total[12] = "";
    char tx_total[12] = "";
    char errors[12] = "";

    meshcore_telemetry_raw_bat(t, raw_bat, sizeof(raw_bat));
    meshcore_telemetry_raw_radio(t, raw_radio, sizeof(raw_radio));
    meshcore_telemetry_raw_pkts(t, raw_pkts, sizeof(raw_pkts));
    meshcore_telemetry_raw_core(t, raw_core, sizeof(raw_core));

    if(t->have_battery) {
        /* meshlog.py put the reference client's "level" in batt_pct, and that
         * value is millivolts despite the column name. Kept identical here so
         * the two sets of logs stay comparable; `voltage` is the same number
         * in the unit the name promises. */
        snprintf(batt, sizeof(batt), "%u", (unsigned)t->battery_mv);
        meshcore_telemetry_volts(t->battery_mv, voltage, sizeof(voltage));
    }

    if(t->have_radio) {
        snprintf(noise, sizeof(noise), "%d", (int)t->noise_floor);
    }

    if(t->have_packets) {
        snprintf(rx_total, sizeof(rx_total), "%lu", (unsigned long)t->recv);
        snprintf(tx_total, sizeof(tx_total), "%lu", (unsigned long)t->sent);
        /* recv_errors arrived only in firmware v1.12. An older node leaves the
         * column empty instead of claiming a confident zero. */
        if(t->has_recv_errors) {
            snprintf(errors, sizeof(errors), "%lu", (unsigned long)t->recv_errors);
        }
    }

    /* raw_core is last, after the meshlog.py columns, so their order and
     * position are untouched — it sits alongside the node/role/hw tags as a
     * Flipper-only trailing extension. */
    int written = snprintf(
        out,
        cap,
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,,%s,%s,%s,%s",
        ts,
        batt,
        voltage,
        noise,
        rx_total,
        tx_total,
        errors,
        lat,
        lon,
        raw_bat,
        raw_radio,
        raw_pkts,
        raw_core);

    return (written < 0) ? 0 : (size_t)written;
}
