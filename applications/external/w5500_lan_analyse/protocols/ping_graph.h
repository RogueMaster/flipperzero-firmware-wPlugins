#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Maximum RTT samples to keep in ring buffer (>= screen width of 128) */
#define PING_GRAPH_MAX_SAMPLES 128

/* Ping interval in ms */
#define PING_GRAPH_INTERVAL_MS 1000

/* Ping timeout per packet in ms */
#define PING_GRAPH_TIMEOUT_MS 2000

/* Value used to mark timeout (no reply) */
#define PING_RTT_TIMEOUT 0xFFFFFFFF

/* Ping graph state */
typedef struct {
    uint32_t samples[PING_GRAPH_MAX_SAMPLES]; /* RTT in ms, PING_RTT_TIMEOUT = loss */
    uint16_t sample_count; /* Total samples recorded */
    uint16_t write_idx; /* Next write position in ring buffer */
    uint32_t total_sent;
    uint32_t total_received;
    uint32_t rtt_min;
    uint32_t rtt_max;
    uint64_t rtt_sum; /* For avg calculation */
    bool running;
} PingGraphState;

/**
 * Initialize ping graph state.
 */
void ping_graph_init(PingGraphState* state);

/**
 * Record a new ping result.
 * rtt_ms: round-trip time in ms, or PING_RTT_TIMEOUT for timeout.
 */
void ping_graph_add_sample(PingGraphState* state, uint32_t rtt_ms);

/**
 * Get the number of displayable samples.
 */
uint16_t ping_graph_visible_count(const PingGraphState* state);

/**
 * Get a sample by display index (0 = oldest visible, count-1 = newest).
 */
uint32_t ping_graph_get_sample(const PingGraphState* state, uint16_t display_idx);

/**
 * Get packet loss percentage (0-100).
 */
uint8_t ping_graph_loss_percent(const PingGraphState* state);

/**
 * Get average RTT (only counting successful pings).
 * Returns 0 if no successful pings.
 */
uint32_t ping_graph_avg_rtt(const PingGraphState* state);
