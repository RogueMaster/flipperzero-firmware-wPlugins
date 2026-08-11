#include "meshcore_rxlog.h"

#include <stdio.h>

bool meshcore_rxlog_parse(const uint8_t* payload, size_t len, MeshCoreRxLog* out) {
    /* code + snr + rssi, then however much packet the radio heard. A frame
     * with no packet body is still valid — the radio can log a runt. */
    if(len < 3) return false;
    if(payload[0] != MESHCORE_RXLOG_CODE) return false;

    out->snr_q4 = (int8_t)payload[1];
    out->rssi = (int8_t)payload[2];
    out->raw = payload + 3;
    out->raw_len = len - 3;
    return true;
}

size_t meshcore_hex_encode(const uint8_t* data, size_t len, char* out, size_t cap) {
    static const char digits[] = "0123456789abcdef";

    if(cap == 0) return 0;

    size_t written = 0;
    for(size_t i = 0; i < len; i++) {
        /* Two characters plus the terminator have to fit, or stop here. */
        if(written + 3 > cap) break;
        out[written++] = digits[(data[i] >> 4) & 0x0F];
        out[written++] = digits[data[i] & 0x0F];
    }

    out[written] = '\0';
    return written;
}

void meshcore_rxlog_format_snr(int8_t snr_q4, char* out, size_t cap) {
    int value = snr_q4;
    const char* sign = (value < 0) ? "-" : "";
    /* Negate as unsigned: -(-128) overflows a signed int8 round trip. */
    unsigned magnitude = (unsigned)(value < 0 ? -(int)value : value);

    snprintf(out, cap, "%s%u.%02u", sign, magnitude / 4u, (magnitude % 4u) * 25u);
}

MeshCoreSnrGrade meshcore_rxlog_grade_snr(int8_t snr_q4) {
    /* Thresholds in quarter-dB, the unit the value arrives in: +5 dB is 20,
     * 0 dB is 0. >= +5 is good, [0, +5) is marginal, below 0 is bad. The +5
     * line is the acceptance criterion the field guides are written around. */
    if(snr_q4 >= 20) return MeshCoreSnrGood;
    if(snr_q4 >= 0) return MeshCoreSnrMarginal;
    return MeshCoreSnrBad;
}

const char* meshcore_rxlog_grade_label(MeshCoreSnrGrade grade) {
    switch(grade) {
    case MeshCoreSnrGood:
        return "GOOD";
    case MeshCoreSnrMarginal:
        return "OK";
    case MeshCoreSnrBad:
        return "WEAK";
    default:
        return "?";
    }
}
