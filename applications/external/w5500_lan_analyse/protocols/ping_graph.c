#include "ping_graph.h"
#include <string.h>

void ping_graph_init(PingGraphState* state) {
    memset(state, 0, sizeof(PingGraphState));
    state->rtt_min = UINT32_MAX;
    state->running = true;
}

void ping_graph_add_sample(PingGraphState* state, uint32_t rtt_ms) {
    state->samples[state->write_idx] = rtt_ms;
    state->write_idx = (state->write_idx + 1) % PING_GRAPH_MAX_SAMPLES;
    if(state->sample_count < PING_GRAPH_MAX_SAMPLES) {
        state->sample_count++;
    }

    state->total_sent++;
    if(rtt_ms != PING_RTT_TIMEOUT) {
        state->total_received++;
        state->rtt_sum += rtt_ms;
        if(rtt_ms < state->rtt_min) state->rtt_min = rtt_ms;
        if(rtt_ms > state->rtt_max) state->rtt_max = rtt_ms;
    }
}

uint16_t ping_graph_visible_count(const PingGraphState* state) {
    return state->sample_count;
}

uint32_t ping_graph_get_sample(const PingGraphState* state, uint16_t display_idx) {
    if(display_idx >= state->sample_count) return PING_RTT_TIMEOUT;

    uint16_t start;
    if(state->sample_count < PING_GRAPH_MAX_SAMPLES) {
        start = 0;
    } else {
        start = state->write_idx; /* Oldest sample position */
    }

    uint16_t actual_idx = (start + display_idx) % PING_GRAPH_MAX_SAMPLES;
    return state->samples[actual_idx];
}

uint8_t ping_graph_loss_percent(const PingGraphState* state) {
    if(state->total_sent == 0) return 0;
    uint32_t lost = state->total_sent - state->total_received;
    return (uint8_t)((lost * 100) / state->total_sent);
}

uint32_t ping_graph_avg_rtt(const PingGraphState* state) {
    if(state->total_received == 0) return 0;
    return (uint32_t)(state->rtt_sum / state->total_received);
}
