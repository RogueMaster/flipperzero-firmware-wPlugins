#pragma once

#include "pmkid_report.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define G5_SESSION_ID_MAX 32
#define G5_AUDIT_BASENAME_MAX 64
#define G5_CAPTURE_SCHEMA "5ghost.capture.v1"
#define G5_AUDIT_SCHEMA "5ghost.audit.v1"

typedef enum {
    G5AuditFileCapturePcap = 0,
    G5AuditFileCaptureJson,
    G5AuditFilePmkid22000,
    G5AuditFilePmkidJson,
    G5AuditFileAuditJson,
} G5AuditFileKind;

typedef enum {
    G5AuditPathNone = 0,
    G5AuditPathHs,
    G5AuditPathPmkid,
} G5AuditPath;

typedef enum {
    G5AuditResultNone = 0,
    G5AuditResultComplete,
    G5AuditResultPartial,
    G5AuditResultUnsupported,
    G5AuditResultTimeout,
    G5AuditResultBlocked,
} G5AuditResult;

typedef struct {
    const char* session_id;
    const char* ssid;
    const char* bssid;
    int channel;
    const char* fw_version;
    const char* fw_caps;
    const char* result;
    const char* essid_hex;
    const char* pmkid_hex;
    const char* reason;
    bool have_22000;
} G5PmkidJsonIn;

typedef struct {
    const char* session_id;
    const char* ssid;
    const char* bssid;
    int channel;
    int pmf;
    const char* fw_version;
    const char* fw_caps;
    G5AuditPath path;
    G5AuditResult result;
    bool have_capture_pcap;
    bool have_capture_json;
    bool have_pmkid_22000;
    bool have_pmkid_json;
} G5AuditJsonIn;

bool g5_session_id_valid(const char* session_id);

size_t g5_session_format_id(
    char* out,
    size_t cap,
    unsigned year,
    unsigned month,
    unsigned day,
    unsigned hour,
    unsigned minute,
    unsigned second,
    uint32_t tick);

size_t g5_audit_basename(
    G5AuditFileKind kind,
    const char* session_id,
    char* out,
    size_t cap);

size_t g5_audit_join_path(
    const char* dir,
    const char* basename,
    char* out,
    size_t cap);

const char* g5_audit_path_name(G5AuditPath path);
const char* g5_audit_result_name(G5AuditResult result);

size_t g5_audit_build_pmkid_json(const G5PmkidJsonIn* in, char* out, size_t cap);
size_t g5_audit_build_audit_json(const G5AuditJsonIn* in, char* out, size_t cap);

typedef enum {
    G5AuditNextStop = 0,
    G5AuditNextStascan,
    G5AuditNextCapture,
    G5AuditNextPmkid,
} G5AuditNext;

typedef struct {
    bool listap_ok;
    int channel;
    int pmf;
    bool stascan_done;
    bool stascan_timed_out;
    int sta_count;
} G5AuditFacts;

typedef struct {
    G5AuditNext next;
    G5AuditPath path;
    G5AuditResult result;
} G5AuditDecision;

bool g5_audit_valid_tx_ch(int ch);
G5AuditDecision g5_audit_decide(const G5AuditFacts* facts);
const char* g5_audit_next_name(G5AuditNext next);
const char* g5_audit_result_label(G5AuditResult result);
const char* g5_audit_tx_verb(G5AuditNext next);
G5AuditResult g5_audit_map_hs(bool quality_valid, bool have_pcap, bool have_json);
G5AuditResult g5_audit_map_pmkid(
    G5PmkidState state,
    const char* reason,
    bool have_22000,
    bool have_json);
