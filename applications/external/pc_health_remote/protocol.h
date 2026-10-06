#pragma once
// PC Health Remote wire protocol v1 (see docs/PROTOCOL.md). Pure C, no firmware deps.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHR_MAGIC0         0x50
#define PHR_MAGIC1         0x48
#define PHR_VERSION        1
#define PHR_TYPE_TELEMETRY 0x01
#define PHR_TYPE_HELLO     0x81
#define PHR_TELEMETRY_LEN  48
#define PHR_HELLO_LEN      12
#define PHR_APP_VERSION    100

#define PHR_FLAG_ON_BATTERY      (1u << 0)
#define PHR_FLAG_CHARGING        (1u << 1)
#define PHR_FLAG_CPU_TEMP_VALID  (1u << 2)
#define PHR_FLAG_GPU_PRESENT     (1u << 3)
#define PHR_FLAG_GPU_TEMP_VALID  (1u << 4)
#define PHR_FLAG_BATTERY_PRESENT (1u << 5)
#define PHR_FLAG_FAN_VALID       (1u << 6)

typedef struct {
    uint8_t seq;
    uint8_t flags;
    uint8_t cpu_load, cpu_temp, gpu_load, gpu_temp;
    uint8_t ram_load, vram_load, disk_load, battery;
    uint16_t ram_total_dgb, vram_total_dgb, cpu_clock_mhz, fan_rpm;
    uint8_t top_cpu_pct, top_ram_pct;
    char top_cpu_name[13]; // NUL-terminated copy of 12 byte field
    char top_ram_name[9]; // NUL-terminated copy of 8 byte field
    uint16_t uptime_h;
} PhrTelemetry;

uint16_t phr_crc16(const uint8_t* data, size_t len);

/** Parse one complete 48-byte frame. Returns false on bad magic/version/type/crc. */
bool phr_parse_telemetry(const uint8_t* buf, size_t len, PhrTelemetry* out);

/** Build HELLO into buf (>= PHR_HELLO_LEN). Returns frame length. */
size_t phr_build_hello(uint8_t* buf, uint8_t seq, uint8_t transport, uint8_t interval_s);

/** Stream reassembler: feed arbitrary byte chunks, resyncs on magic. */
typedef struct {
    uint8_t buf[PHR_TELEMETRY_LEN];
    size_t fill;
} PhrStream;

void phr_stream_reset(PhrStream* s);
/** Feeds data; for every valid telemetry frame calls cb(frame, ctx). Returns frames delivered. */
size_t phr_stream_feed(
    PhrStream* s,
    const uint8_t* data,
    size_t len,
    void (*cb)(const PhrTelemetry*, void*),
    void* ctx);
