#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Port scanner socket (uses sockets 3-7 for parallel scanning) */
#define W5500_SCAN_SOCKET_BASE  3
#define W5500_SCAN_SOCKET_COUNT 1 /* Use single socket for simplicity */

/* Connect timeout per port in ms */
#define PORT_SCAN_TIMEOUT_MS 1500

/* Port states */
typedef enum {
    PortStateOpen,
    PortStateClosed,
    PortStateFiltered,
} PortState;

/* Result for a single port */
typedef struct {
    uint16_t port;
    PortState state;
} PortScanResult;

/* Maximum ports to store results for */
#define PORT_SCAN_MAX_RESULTS 128

/* Preset port lists */
#define PORT_PRESET_TOP20_COUNT 18
extern const uint16_t PORT_PRESET_TOP20[];

#define PORT_PRESET_TOP100_COUNT 100
extern const uint16_t PORT_PRESET_TOP100[];

/**
 * Scan a single TCP port using W5500 socket.
 * socket_num: W5500 socket to use
 * target_ip: destination IP
 * port: target port
 * timeout_ms: connection timeout
 * Returns the port state.
 */
PortState port_scan_tcp(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint16_t port,
    uint32_t timeout_ms);
