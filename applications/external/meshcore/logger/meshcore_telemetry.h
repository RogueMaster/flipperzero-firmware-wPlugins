/*
 * telemetry.csv rows.
 *
 * Pure formatting, no furi and no I/O, so the column contract can be tested on
 * a laptop rather than by walking a field and reading a card afterwards.
 *
 * The columns are meshlog.py's, in meshlog.py's order, because Sergey's logs
 * and Mark's are merged with the same script. Two differences are deliberate
 * and documented at the fields themselves: `batt_pct` and the `raw_*` columns.
 *
 * Battery comes from GET_BATT_AND_STORAGE (command 20) when the node answers
 * it, and from the CORE stats block otherwise. Both report millivolts.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool have_battery;
    uint16_t battery_mv;

    bool have_core;
    uint32_t uptime_secs;
    uint16_t core_errors;
    uint8_t queue_len;

    bool have_radio;
    int16_t noise_floor;
    int8_t last_rssi;
    int8_t last_snr_q4;
    uint32_t tx_air_secs;
    uint32_t rx_air_secs;

    bool have_packets;
    uint32_t recv;
    uint32_t sent;
    uint32_t flood_tx;
    uint32_t direct_tx;
    uint32_t flood_rx;
    uint32_t direct_rx;
    uint32_t recv_errors;
    bool has_recv_errors;
} MeshCoreTelemetry;

/** ts,batt_pct,voltage,noise_floor,rx_total,tx_total,recv_errors,lat,lon,acc,
 *  raw_bat,raw_radio,raw_pkts,raw_core — the node tags are appended by the
 *  caller. raw_core (uptime, queue depth, CORE error count) is a Flipper-only
 *  trailing column: meshlog.py never collected the CORE block, so keeping it
 *  after the shared columns leaves their order and position untouched.
 *
 *  A field the node did not report is written empty rather than as zero: on a
 *  discharge curve a missing sample and a real zero mean opposite things.
 *
 *  Returns the number of characters written, excluding the terminator. */
size_t meshcore_telemetry_format(
    const MeshCoreTelemetry* telemetry,
    const char* ts,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap);

#define MESHCORE_TELEMETRY_HEADER                                            \
    "ts,batt_pct,voltage,noise_floor,rx_total,tx_total,recv_errors,lat,lon," \
    "acc,raw_bat,raw_radio,raw_pkts,raw_core,node,role,hw"
