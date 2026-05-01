#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ICMP types */
#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

/* Ping result */
typedef struct {
    uint8_t target_ip[4];
    uint16_t seq;
    uint32_t rtt_ms;
    bool success;
} PingResult;

/**
 * Send a single ICMP echo request and wait for reply.
 * Uses W5500 IPRAW socket (socket_num).
 * target_ip: destination IP
 * seq: sequence number
 * timeout_ms: max wait time
 * result: output
 * Returns true if reply received.
 */
/**
 * @param running  If non-NULL, checked each ms — ping aborts when *running becomes false.
 */
bool icmp_ping(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint16_t seq,
    uint32_t timeout_ms,
    PingResult* result,
    const volatile bool* running);
