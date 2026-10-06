#include "protocol.h"
#include <string.h>

uint16_t phr_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

bool phr_parse_telemetry(const uint8_t* b, size_t len, PhrTelemetry* out) {
    if(len != PHR_TELEMETRY_LEN) return false;
    if(b[0] != PHR_MAGIC0 || b[1] != PHR_MAGIC1 || b[2] != PHR_VERSION ||
       b[3] != PHR_TYPE_TELEMETRY)
        return false;
    if(phr_crc16(b, PHR_TELEMETRY_LEN - 2) != rd16(b + 46)) return false;
    if(!out) return true;
    memset(out, 0, sizeof(*out));
    out->seq = b[4];
    out->flags = b[5];
    out->cpu_load = b[6];
    out->cpu_temp = b[7];
    out->gpu_load = b[8];
    out->gpu_temp = b[9];
    out->ram_load = b[10];
    out->vram_load = b[11];
    out->disk_load = b[12];
    out->battery = b[13];
    out->ram_total_dgb = rd16(b + 14);
    out->vram_total_dgb = rd16(b + 16);
    out->cpu_clock_mhz = rd16(b + 18);
    out->fan_rpm = rd16(b + 20);
    out->top_cpu_pct = b[22];
    out->top_ram_pct = b[23];
    // Names: keep printable ASCII only, stop at first NUL.
    for(int i = 0; i < 12 && b[24 + i]; i++) {
        uint8_t c = b[24 + i];
        out->top_cpu_name[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    for(int i = 0; i < 8 && b[36 + i]; i++) {
        uint8_t c = b[36 + i];
        out->top_ram_name[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out->uptime_h = rd16(b + 44);
    return true;
}

size_t phr_build_hello(uint8_t* b, uint8_t seq, uint8_t transport, uint8_t interval_s) {
    b[0] = PHR_MAGIC0;
    b[1] = PHR_MAGIC1;
    b[2] = PHR_VERSION;
    b[3] = PHR_TYPE_HELLO;
    b[4] = seq;
    b[5] = PHR_APP_VERSION & 0xFF;
    b[6] = PHR_APP_VERSION >> 8;
    b[7] = transport;
    b[8] = interval_s;
    b[9] = 0;
    uint16_t crc = phr_crc16(b, 10);
    b[10] = crc & 0xFF;
    b[11] = crc >> 8;
    return PHR_HELLO_LEN;
}

void phr_stream_reset(PhrStream* s) {
    s->fill = 0;
}

// Drop the first `n` bytes of the buffer.
static void drop(PhrStream* s, size_t n) {
    if(n >= s->fill) {
        s->fill = 0;
        return;
    }
    memmove(s->buf, s->buf + n, s->fill - n);
    s->fill -= n;
}

// Discard bytes until buffer begins with (a prefix of) the magic.
static void resync(PhrStream* s) {
    size_t i = 0;
    while(i < s->fill) {
        if(s->buf[i] == PHR_MAGIC0) {
            if(i + 1 >= s->fill || s->buf[i + 1] == PHR_MAGIC1) break;
        }
        i++;
    }
    if(i) drop(s, i);
}

size_t phr_stream_feed(
    PhrStream* s,
    const uint8_t* data,
    size_t len,
    void (*cb)(const PhrTelemetry*, void*),
    void* ctx) {
    size_t frames = 0;
    for(size_t i = 0; i < len; i++) {
        s->buf[s->fill++] = data[i];
        resync(s);
        // Early header validation so garbage does not stall a full frame.
        if(s->fill >= 4 && (s->buf[2] != PHR_VERSION || s->buf[3] != PHR_TYPE_TELEMETRY)) {
            drop(s, 1);
            resync(s);
            continue;
        }
        if(s->fill == PHR_TELEMETRY_LEN) {
            PhrTelemetry t;
            if(phr_parse_telemetry(s->buf, s->fill, &t)) {
                if(cb) cb(&t, ctx);
                frames++;
                s->fill = 0;
            } else {
                drop(s, 1); // bad CRC: maybe the magic was inside payload; rescan
                resync(s);
            }
        }
    }
    return frames;
}
