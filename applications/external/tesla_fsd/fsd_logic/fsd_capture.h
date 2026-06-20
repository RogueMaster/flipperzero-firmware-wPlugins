#pragma once
/*
 * fsd_capture.h — candump-ASCII line formatter, shared by both platforms.
 *
 * The ESP32 already logs the bus to SD in SocketCAN "candump" ASCII
 * ("(sec.usec) can0 ID#DATA"); the capture-first feature gives the Flipper the
 * same capability. Both produce the exact format that tools/tesla_crc_cracker.py
 * (and any candump tool) reads, so a community capture drops straight into the
 * cracker / a bug report. Keeping the formatter in one header-only function
 * means the two platforms can't drift on the on-disk format.
 *
 *   (<elapsed_sec>.<elapsed_usec>) <bus> <ID>#<DATA>\n
 *   e.g.  (1.500000) can0 485#00112233445566AA
 *
 * Pure: formats into the caller's buffer, does no I/O. Returns the number of
 * bytes written (excluding the trailing NUL), which is what the caller writes
 * to the file. Output is always NUL-terminated.
 */

#include <stdint.h>
#include <stdio.h>

static inline int tesla_format_candump_line(
    char* out,
    int out_sz,
    uint32_t elapsed_ms,
    const char* bus_name,
    uint32_t can_id,
    const uint8_t* data,
    uint8_t dlc) {
    if(out_sz <= 0) return 0;
    uint32_t sec = elapsed_ms / 1000u;
    uint32_t usec = (elapsed_ms % 1000u) * 1000u;
    int pos = snprintf(
        out,
        (size_t)out_sz,
        "(%lu.%06lu) %s %03lX#",
        (unsigned long)sec,
        (unsigned long)usec,
        bus_name,
        (unsigned long)can_id);
    if(pos < 0) {
        out[0] = '\0';
        return 0;
    }
    if(dlc > 8) dlc = 8;
    for(uint8_t i = 0; i < dlc && pos < out_sz - 1; i++)
        pos += snprintf(out + pos, (size_t)(out_sz - pos), "%02X", data[i]);
    if(pos > out_sz - 1) pos = out_sz - 1; // snprintf truncated — clamp
    if(pos < out_sz - 1) out[pos++] = '\n';
    out[pos] = '\0';
    return pos;
}
