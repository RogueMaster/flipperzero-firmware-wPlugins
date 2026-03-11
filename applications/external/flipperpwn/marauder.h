#pragma once

#include <furi.h>
#include "wifi_uart.h"

/* Opaque Marauder command context. */
typedef struct FPwnMarauder FPwnMarauder;

/* --------------------------------------------------------------------------
 * State machine
 * -------------------------------------------------------------------------- */
typedef enum {
    FPwnMarauderStateIdle,
    FPwnMarauderStateScanning,
    FPwnMarauderStateScanStopping, /* stopscan sent; still draining AP results */
    FPwnMarauderStateJoined,
    FPwnMarauderStatePingScan,
    FPwnMarauderStatePortScan,
    FPwnMarauderStateDeauth,
    FPwnMarauderStateSniffPmkid,
    FPwnMarauderStateEvilPortal,
    FPwnMarauderStateBeaconSpam,
    FPwnMarauderStateStationScan, /* scanning client stations */
    FPwnMarauderStateSniffDeauth, /* WPA handshake capture via deauth */
    FPwnMarauderStateSniffProbe, /* sniffing probe requests */
} FPwnMarauderState;

/* --------------------------------------------------------------------------
 * Result types
 * -------------------------------------------------------------------------- */

/* A discovered WiFi access point. */
typedef struct {
    char ssid[33]; /* max 32-char SSID + NUL */
    char bssid[18]; /* "AA:BB:CC:DD:EE:FF" + NUL */
    int8_t rssi;
    uint8_t channel;
    uint8_t encryption; /* 0=open, 1=WEP, 2=WPA, 3=WPA2 */
} FPwnWifiAP;

/* A host found alive on the network. */
typedef struct {
    char ip[16]; /* dotted-quad + NUL */
    bool alive;
} FPwnNetHost;

/* A single port scan result. */
typedef struct {
    uint16_t port;
    bool open;
    char service[16];
} FPwnPortResult;

/* A discovered WiFi client station. */
typedef struct {
    char mac[18]; /* "AA:BB:CC:DD:EE:FF" + NUL */
    char ap_ssid[33]; /* associated AP SSID + NUL */
    int8_t rssi;
} FPwnStation;

/* A captured credential (from evil portal POST data, etc.). */
typedef struct {
    char data[128]; /* raw POST data or credential line */
} FPwnCapturedCred;

/* --------------------------------------------------------------------------
 * Capacity limits
 * -------------------------------------------------------------------------- */
#define FPWN_MAX_APS      64
#define FPWN_MAX_HOSTS    64
#define FPWN_MAX_PORTS    128
#define FPWN_MAX_STATIONS 64
#define FPWN_MAX_CREDS    32

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/* Allocate and bind to an existing UART handle.  Registers itself as the
 * UART RX callback — do not register another callback after this. */
FPwnMarauder* fpwn_marauder_alloc(FPwnWifiUart* uart);

/* Release mutex and free the struct.  Does not free the uart. */
void fpwn_marauder_free(FPwnMarauder* marauder);

/* --------------------------------------------------------------------------
 * Commands — each sends the corresponding Marauder CLI command
 * -------------------------------------------------------------------------- */

/* Begin a passive WiFi AP scan.  Clears the AP list first. */
void fpwn_marauder_scan_ap(FPwnMarauder* m);

/* Send "stopscan" and return to Idle. */
void fpwn_marauder_stop_scan(FPwnMarauder* m);

/* Associate with AP at index `ap_idx`.  Pass empty string for open networks. */
void fpwn_marauder_join(FPwnMarauder* m, uint8_t ap_idx, const char* password);

/* Send an ICMP ping sweep of the current subnet.  Clears host list first. */
void fpwn_marauder_ping_scan(FPwnMarauder* m);

/* Port scan the host at `host_idx`.  `all_ports` adds the "-a" flag. */
void fpwn_marauder_port_scan(FPwnMarauder* m, uint8_t host_idx, bool all_ports);

/* Launch a deauth attack against all visible APs. */
void fpwn_marauder_deauth(FPwnMarauder* m);

/* Sniff for PMKID handshakes. */
void fpwn_marauder_sniff_pmkid(FPwnMarauder* m);

/* Stop any active operation and return to Idle. */
void fpwn_marauder_stop(FPwnMarauder* m);

/* Start an evil portal with the given SSID. Marauder serves a captive portal. */
void fpwn_marauder_evil_portal(FPwnMarauder* m, const char* ssid);

/* Start beacon spam — floods area with fake SSIDs. */
void fpwn_marauder_beacon_spam(FPwnMarauder* m);

/* Scan for associated client stations.  Clears the station list first. */
void fpwn_marauder_scan_sta(FPwnMarauder* m);

/* Sniff for WPA handshakes via deauth injection (raw output to log). */
void fpwn_marauder_sniff_deauth(FPwnMarauder* m);

/* Sniff for 802.11 probe requests (raw output to log). */
void fpwn_marauder_sniff_probe(FPwnMarauder* m);

/* Select a specific AP by index for targeted attacks. */
void fpwn_marauder_select_ap(FPwnMarauder* m, uint8_t ap_idx);

/* Deauth a specific AP (must call select_ap first, or select -a for all). */
void fpwn_marauder_deauth_targeted(FPwnMarauder* m, uint8_t ap_idx);

/* --------------------------------------------------------------------------
 * Accessors — all thread-safe via internal mutex
 * -------------------------------------------------------------------------- */

/* Register a secondary callback invoked for every line received from the
 * UART, after the marauder parser has processed it.  Used by wifi_views.c
 * to populate the status TextBox without interfering with parsing. */
void fpwn_marauder_set_log_callback(FPwnMarauder* m, FPwnWifiRxCallback cb, void* ctx);

FPwnMarauderState fpwn_marauder_get_state(FPwnMarauder* m);

/* Returns a pointer to the internal AP array and sets *count.
 * Valid until the next scan_ap() call. */
FPwnWifiAP* fpwn_marauder_get_aps(FPwnMarauder* m, uint32_t* count);

/* Returns a pointer to the internal host array and sets *count. */
FPwnNetHost* fpwn_marauder_get_hosts(FPwnMarauder* m, uint32_t* count);

/* Returns a pointer to the internal port array and sets *count. */
FPwnPortResult* fpwn_marauder_get_ports(FPwnMarauder* m, uint32_t* count);

/* Returns a pointer to the internal station array and sets *count.
 * Valid until the next scan_sta() call. */
FPwnStation* fpwn_marauder_get_stations(FPwnMarauder* m, uint32_t* count);

/* Returns a pointer to the captured credentials array and sets *count.
 * Populated by evil portal POST data and similar captures. */
FPwnCapturedCred* fpwn_marauder_get_creds(FPwnMarauder* m, uint32_t* count);

/* Returns the furi_get_tick() value from when the current AP scan started.
 * Used by the timer callback to implement auto-stop after 8 seconds. */
uint32_t fpwn_marauder_get_scan_start(FPwnMarauder* m);
