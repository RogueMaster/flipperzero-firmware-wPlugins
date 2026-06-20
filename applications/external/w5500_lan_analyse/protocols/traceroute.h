#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Traceroute socket on W5500 */
#define W5500_TRACEROUTE_SOCKET 2

/* Max TTL */
#define TRACEROUTE_MAX_TTL 30

/* Timeout per hop in ms */
#define TRACEROUTE_HOP_TIMEOUT_MS 2000

/* ICMP types */
#define ICMP_TIME_EXCEEDED 11
#define ICMP_ECHO_REPLY    0

/* Result for a single hop */
typedef struct {
    uint8_t ttl;
    uint8_t hop_ip[4];
    uint32_t rtt_ms;
    bool responded;
    bool is_destination;
} TracerouteHop;

/**
 * Send a single ICMP echo request with given TTL and wait for reply.
 * socket_num: W5500 IPRAW socket
 * target_ip: final destination
 * ttl: Time-To-Live value
 * seq: sequence number
 * timeout_ms: max wait
 * hop: output
 * Returns true if any ICMP response received.
 */
bool traceroute_send_hop(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint8_t ttl,
    uint16_t seq,
    uint32_t timeout_ms,
    TracerouteHop* hop);
