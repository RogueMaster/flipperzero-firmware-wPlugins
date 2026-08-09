#pragma once
#include <stdint.h>

typedef enum {
    WINDOWS,
    LINUX,
    IOS,
    NO_DETECTED,
} OS_DETECTOR_OS;

typedef enum {
    OFP_UNSET,
    OFP_TSEQ,
    OFP_TOPS,
    OFP_TECN,
    OFP_T1_7,
    OFP_TICMP,
    OFP_TUDP,
} OFP_PROBES_TYPE;

typedef enum {
    IPID_UNKNOWN = 0,
    IPID_CONSTANT,
    IPID_INCREMENTAL,
    IPID_INCREMENTAL_LARGE,
    IPID_RANDOM,
    IPID_ZERO
} ipid_pattern_t;

typedef struct {
    int windows_score;
    int linux_score;
    int ios_score;
} os_scoreboard_t;

typedef struct {
    bool has_mss;
    bool has_sack;
    bool has_ws;
    bool has_ts;
    bool has_nop;

    uint8_t options_length; // longitud total de opciones TCP
    uint32_t tsval; // timestamp value
    uint32_t tsecr; // timestamp echo reply

    uint8_t order[16];
    uint8_t count;

    uint16_t mss_value;
    uint8_t ws_value;

} tcp_opts_t;

typedef enum {
    PORT_UNKNOWN = 0,
    PORT_OPEN,
    PORT_CLOSED,
    PORT_FILTERED
} port_state_t;

typedef struct {
    uint16_t port;
    port_state_t state;
    uint32_t last_sequence;
    uint16_t window_size;
    uint8_t ttl;
} port_result_t;

int32_t os_scan(void* context, uint8_t* target_ip);
