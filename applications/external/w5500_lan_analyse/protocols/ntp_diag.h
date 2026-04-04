#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t leap; /* 0=none, 1=+1s, 2=-1s, 3=unsync */
    uint8_t version;
    uint8_t mode; /* 4=server */
    uint8_t stratum; /* 0=unspec, 1=primary, 2-15=secondary */
    int8_t poll; /* log2 poll interval */
    int8_t precision; /* log2 precision */
    uint32_t root_delay; /* fixed-point 16.16 */
    uint32_t root_disp; /* fixed-point 16.16 */
    uint32_t ref_id; /* reference ID */
    char ref_id_str[16]; /* human-readable ref ID (stratum 1) */
    int32_t offset_us; /* clock offset in microseconds */
    uint32_t rtt_us; /* round-trip time in microseconds */
    char stratum_name[24]; /* human-readable stratum description */
    bool valid;
} NtpDiagResult;

/**
 * Send NTP query and analyze response.
 * @param server_ip  NTP server IP
 * @param result     Output structure
 * @return true if valid response received
 */
bool ntp_diag_query(const uint8_t server_ip[4], NtpDiagResult* result);
