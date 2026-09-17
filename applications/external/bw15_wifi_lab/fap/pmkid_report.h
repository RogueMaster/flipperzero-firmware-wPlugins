// fap/pmkid_report.h — 5Ghost B3: parse firmware "PMKID:" output lines, classify
// into 6 quality states, and assemble a hashcat 22000 (WPA*01) record.
//
// Pure logic, NO furi/flipper deps -> host-compilable for offline unit tests
// (tools/tests/test_pmkid_report.{c,py}). Mirrors the firmware contract emitted
// by firmware/5ghost-fw/src/wifi_pmkid/pmkid.cpp:
//   PMKID:on ch=<n>                         (advisory, non-terminal)
//   PMKID:st=<sync|auth|assoc|waitm1>       (progress, non-terminal)
//   PMKID:ok bssid=AA:.. sta=AA:.. essid_hex=<hex> pmkid=<32hex>   (terminal)
//   PMKID:fail=<reason>                     (terminal)
//   PMKID:err=<args|mac|chan|state|arm>     (terminal, command-layer reject)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    G5PmkidPending = 0,   // on/st progress line — not a terminal state
    G5PmkidValid,         // ok + target match + pmkid/essid sane + new this session
    G5PmkidDuplicate,     // ok but same BSSID already captured this session
    G5PmkidWrongTarget,   // ok but returned bssid != requested target (defensive)
    G5PmkidPartial,       // fail=auth-rejected / assoc-rejected (AP actively refused)
    G5PmkidTimeout,       // fail=no-beacon / no-auth / no-assoc / no-m1 (never arrived)
    G5PmkidUnsupported,   // fail=no-pmkid / pmf-required / unsupported-* / hidden-ssid /
                          // *-rsn / no-rates / invalid-pmkid (AP unfit for clientless PMKID)
    G5PmkidError,         // err=* (command-layer rejection)
} G5PmkidState;

typedef struct {
    bool ok;              // true once a PMKID:ok terminal line has been parsed
    uint8_t bssid[6];     // AP MAC (from ok line)
    uint8_t sta[6];       // our STA MAC (from ok line)
    char essid_hex[65];   // raw beacon SSID bytes as hex, <=64 chars + NUL
    char pmkid_hex[33];   // 32 hex chars + NUL
    char reason[24];      // fail=/err= reason with prefix stripped
    G5PmkidState state;   // classification (see parse vs classify note below)
} G5PmkidReport;

// Parse one firmware output line into *out. Returns true when the line is a
// TERMINAL state (ok / fail / err); false for progress (on / st -> state set to
// G5PmkidPending) and for any non-"PMKID:" line (out left unchanged).
//
// For an ok line, out->state is set to G5PmkidValid PROVISIONALLY; the caller
// MUST refine it via g5_pmkid_classify_ok(), which knows the requested target and
// the already-seen set and runs the secondary degenerate-pmkid guard.
bool g5_pmkid_parse_line(const char* line, G5PmkidReport* out);

// Refine an ok report's final state using the requested target BSSID and whether
// this BSSID was already captured this session. Enforces (in priority order):
// wrong-target > degenerate pmkid (all-0/all-FF or not 32 hex) or bad essid_hex >
// duplicate > valid. Returns G5PmkidError if the report is not an ok report.
G5PmkidState g5_pmkid_classify_ok(
    const G5PmkidReport* report, const uint8_t requested_bssid[6], bool already_seen);

// Assemble a hashcat 22000 WPA*01 record (no trailing newline) into out[cap]:
//   WPA*01*<pmkid32>*<bssid12>*<sta12>*<essid_hex>***
// Validates: report->ok, pmkid is exactly 32 hex and not all-0/all-FF, essid_hex is
// non-empty even-length hex. bssid/sta are emitted lowercase. Returns bytes written
// (excluding NUL), or 0 on any validation failure or truncation.
size_t g5_pmkid_build_22000(const G5PmkidReport* report, char* out, size_t cap);
