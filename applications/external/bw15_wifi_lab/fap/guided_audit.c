#include "guided_audit.h"
#include "evidence_pack.h"

#include <stdio.h>
#include <string.h>

static bool json_field_safe(const char* s) {
    if(!s) return false;
    for(; *s; ++s) {
        if(*s == '"' || *s == '\\' || (unsigned char)*s < 0x20) return false;
    }
    return true;
}

static bool session_char_ok(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           c == '_';
}

bool g5_session_id_valid(const char* session_id) {
    if(!session_id || session_id[0] == '\0') return false;
    size_t n = 0;
    for(; session_id[n] != '\0'; ++n) {
        if(n + 1 >= G5_SESSION_ID_MAX) return false;
        if(!session_char_ok((unsigned char)session_id[n])) return false;
    }
    return n > 0;
}

size_t g5_session_format_id(
    char* out,
    size_t cap,
    unsigned year,
    unsigned month,
    unsigned day,
    unsigned hour,
    unsigned minute,
    unsigned second,
    uint32_t tick) {
    if(!out || cap < 25) return 0;
    const int n = snprintf(
        out,
        cap,
        "%04u%02u%02u_%02u%02u%02u_%08lX",
        year,
        month,
        day,
        hour,
        minute,
        second,
        (unsigned long)tick);
    if(n < 0 || (size_t)n >= cap) {
        if(cap) out[0] = '\0';
        return 0;
    }
    if(!g5_session_id_valid(out)) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

size_t g5_audit_basename(
    G5AuditFileKind kind,
    const char* session_id,
    char* out,
    size_t cap) {
    if(!out || cap == 0) return 0;
    out[0] = '\0';
    if(!g5_session_id_valid(session_id)) return 0;

    const char* fmt = NULL;
    switch(kind) {
    case G5AuditFileCapturePcap:
        fmt = "capture_%s.pcap";
        break;
    case G5AuditFileCaptureJson:
        fmt = "capture_%s.json";
        break;
    case G5AuditFilePmkid22000:
        fmt = "pmkid_%s.22000";
        break;
    case G5AuditFilePmkidJson:
        fmt = "pmkid_%s.json";
        break;
    case G5AuditFileAuditJson:
        fmt = "audit_%s.json";
        break;
    default:
        return 0;
    }

    const int n = snprintf(out, cap, fmt, session_id);
    if(n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

size_t g5_audit_join_path(
    const char* dir,
    const char* basename,
    char* out,
    size_t cap) {
    if(!out || cap == 0) return 0;
    out[0] = '\0';
    if(!dir || dir[0] == '\0' || !basename || basename[0] == '\0') return 0;
    if(strchr(basename, '/') || strchr(basename, '\\')) return 0;
    const int n = snprintf(out, cap, "%s/%s", dir, basename);
    if(n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

const char* g5_audit_path_name(G5AuditPath path) {
    switch(path) {
    case G5AuditPathHs:
        return "hs";
    case G5AuditPathPmkid:
        return "pmkid";
    case G5AuditPathNone:
        return "none";
    }
    return "none";
}

const char* g5_audit_result_name(G5AuditResult result) {
    switch(result) {
    case G5AuditResultComplete:
        return "complete";
    case G5AuditResultPartial:
        return "partial";
    case G5AuditResultUnsupported:
        return "unsupported";
    case G5AuditResultTimeout:
        return "timeout";
    case G5AuditResultBlocked:
        return "blocked";
    case G5AuditResultNone:
        return "none";
    }
    return "none";
}

static const char* or_empty(const char* s) {
    return s ? s : "";
}

static bool escape_or_fail(
    const char* input,
    char* output,
    size_t output_capacity,
    size_t* output_length) {
    return g5_evidence_json_escape(or_empty(input), output, output_capacity, output_length);
}

static const char* safe_or_blank(const char* s) {
    return json_field_safe(or_empty(s)) ? or_empty(s) : "";
}

size_t g5_audit_build_pmkid_json(const G5PmkidJsonIn* in, char* out, size_t cap) {
    if(!in || !out || cap == 0) return 0;
    out[0] = '\0';
    if(!g5_session_id_valid(in->session_id)) return 0;
    if(!in->result || !json_field_safe(in->result)) return 0;
    if(!in->bssid || !json_field_safe(in->bssid)) return 0;

    char escaped_ssid[96];
    char escaped_fw[48];
    char escaped_caps[32];
    size_t escaped_length = 0;
    if(!escape_or_fail(in->ssid, escaped_ssid, sizeof(escaped_ssid), &escaped_length))
        return 0;
    if(!escape_or_fail(in->fw_version, escaped_fw, sizeof(escaped_fw), &escaped_length))
        return 0;
    if(!escape_or_fail(in->fw_caps, escaped_caps, sizeof(escaped_caps), &escaped_length))
        return 0;

    char hc_name[G5_AUDIT_BASENAME_MAX];
    hc_name[0] = '\0';
    if(in->have_22000) {
        if(g5_audit_basename(G5AuditFilePmkid22000, in->session_id, hc_name, sizeof(hc_name)) == 0)
            return 0;
    }

    const int n = snprintf(
        out,
        cap,
        "{\n"
        "  \"schema\":\"%s\",\n"
        "  \"session_id\":\"%s\",\n"
        "  \"kind\":\"pmkid\",\n"
        "  \"result\":\"%s\",\n"
        "  \"target\":{\"ssid\":\"%s\",\"bssid\":\"%s\",\"channel\":%d},\n"
        "  \"firmware\":{\"version\":\"%s\",\"caps\":\"%s\"},\n"
        "  \"quality\":{\"state\":\"%s\",\"reason\":\"%s\",\"essid_hex\":\"%s\",\"pmkid\":\"%s\"},\n"
        "  \"artifacts\":{\"hc22000\":%s%s%s}\n"
        "}\n",
        G5_CAPTURE_SCHEMA,
        in->session_id,
        in->result,
        escaped_ssid,
        in->bssid,
        in->channel,
        escaped_fw,
        escaped_caps,
        in->result,
        safe_or_blank(in->reason),
        safe_or_blank(in->essid_hex),
        safe_or_blank(in->pmkid_hex),
        in->have_22000 ? "\"" : "",
        in->have_22000 ? hc_name : "null",
        in->have_22000 ? "\"" : "");
    if(n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

size_t g5_audit_build_audit_json(const G5AuditJsonIn* in, char* out, size_t cap) {
    if(!in || !out || cap == 0) return 0;
    out[0] = '\0';
    if(!g5_session_id_valid(in->session_id)) return 0;
    if(!in->bssid || !json_field_safe(in->bssid)) return 0;

    char escaped_ssid[96];
    char escaped_fw[48];
    char escaped_caps[32];
    size_t escaped_length = 0;
    if(!escape_or_fail(in->ssid, escaped_ssid, sizeof(escaped_ssid), &escaped_length))
        return 0;
    if(!escape_or_fail(in->fw_version, escaped_fw, sizeof(escaped_fw), &escaped_length))
        return 0;
    if(!escape_or_fail(in->fw_caps, escaped_caps, sizeof(escaped_caps), &escaped_length))
        return 0;

    char capture_pcap[G5_AUDIT_BASENAME_MAX];
    char capture_json[G5_AUDIT_BASENAME_MAX];
    char pmkid_22000[G5_AUDIT_BASENAME_MAX];
    char pmkid_json[G5_AUDIT_BASENAME_MAX];
    char audit_name[G5_AUDIT_BASENAME_MAX];
    if(g5_audit_basename(G5AuditFileAuditJson, in->session_id, audit_name, sizeof(audit_name)) == 0)
        return 0;
    if(in->have_capture_pcap &&
       g5_audit_basename(
           G5AuditFileCapturePcap, in->session_id, capture_pcap, sizeof(capture_pcap)) == 0)
        return 0;
    if(in->have_capture_json &&
       g5_audit_basename(
           G5AuditFileCaptureJson, in->session_id, capture_json, sizeof(capture_json)) == 0)
        return 0;
    if(in->have_pmkid_22000 &&
       g5_audit_basename(
           G5AuditFilePmkid22000, in->session_id, pmkid_22000, sizeof(pmkid_22000)) == 0)
        return 0;
    if(in->have_pmkid_json &&
       g5_audit_basename(G5AuditFilePmkidJson, in->session_id, pmkid_json, sizeof(pmkid_json)) ==
           0)
        return 0;

    const int n = snprintf(
        out,
        cap,
        "{\n"
        "  \"schema\":\"%s\",\n"
        "  \"session_id\":\"%s\",\n"
        "  \"kind\":\"guided_audit\",\n"
        "  \"path\":\"%s\",\n"
        "  \"result\":\"%s\",\n"
        "  \"target\":{\"ssid\":\"%s\",\"bssid\":\"%s\",\"channel\":%d,\"pmf\":%d},\n"
        "  \"firmware\":{\"version\":\"%s\",\"caps\":\"%s\"},\n"
        "  \"artifacts\":{\"audit\":\"%s\",\"pcap\":%s%s%s,\"capture_json\":%s%s%s,"
        "\"hc22000\":%s%s%s,\"pmkid_json\":%s%s%s}\n"
        "}\n",
        G5_AUDIT_SCHEMA,
        in->session_id,
        g5_audit_path_name(in->path),
        g5_audit_result_name(in->result),
        escaped_ssid,
        in->bssid,
        in->channel,
        in->pmf,
        escaped_fw,
        escaped_caps,
        audit_name,
        in->have_capture_pcap ? "\"" : "",
        in->have_capture_pcap ? capture_pcap : "null",
        in->have_capture_pcap ? "\"" : "",
        in->have_capture_json ? "\"" : "",
        in->have_capture_json ? capture_json : "null",
        in->have_capture_json ? "\"" : "",
        in->have_pmkid_22000 ? "\"" : "",
        in->have_pmkid_22000 ? pmkid_22000 : "null",
        in->have_pmkid_22000 ? "\"" : "",
        in->have_pmkid_json ? "\"" : "",
        in->have_pmkid_json ? pmkid_json : "null",
        in->have_pmkid_json ? "\"" : "");
    if(n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

/* Must match firmware/5ghost-fw/src/core/channels.h g5_ch_tx[] (22 entries, no DFS). */
static const uint8_t g5_audit_ch_tx[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    36, 40, 44, 48, 149, 153, 157, 161, 165};

bool g5_audit_valid_tx_ch(int ch) {
    if(ch < 0 || ch > 255) return false;
    const uint8_t c = (uint8_t)ch;
    for(size_t i = 0; i < sizeof(g5_audit_ch_tx); i++) {
        if(g5_audit_ch_tx[i] == c) return true;
    }
    return false;
}

G5AuditDecision g5_audit_decide(const G5AuditFacts* facts) {
    G5AuditDecision d = {
        .next = G5AuditNextStop,
        .path = G5AuditPathNone,
        .result = G5AuditResultNone,
    };
    if(!facts || !facts->listap_ok) {
        d.result = G5AuditResultTimeout;
        return d;
    }
    if(!g5_audit_valid_tx_ch(facts->channel)) {
        d.result = G5AuditResultUnsupported;
        return d;
    }
    if(facts->pmf >= 2) {
        d.next = G5AuditNextPmkid;
        d.path = G5AuditPathPmkid;
        return d;
    }
    if(facts->stascan_timed_out) {
        d.result = G5AuditResultTimeout;
        return d;
    }
    if(!facts->stascan_done) {
        d.next = G5AuditNextStascan;
        return d;
    }
    if(facts->sta_count > 0) {
        d.next = G5AuditNextCapture;
        d.path = G5AuditPathHs;
        return d;
    }
    d.next = G5AuditNextPmkid;
    d.path = G5AuditPathPmkid;
    return d;
}

const char* g5_audit_next_name(G5AuditNext next) {
    switch(next) {
    case G5AuditNextStascan:
        return "STA";
    case G5AuditNextCapture:
        return "HS";
    case G5AuditNextPmkid:
        return "PMKID";
    case G5AuditNextStop:
        return "";
    }
    return "";
}

const char* g5_audit_result_label(G5AuditResult result) {
    switch(result) {
    case G5AuditResultComplete:
        return "Complete";
    case G5AuditResultPartial:
        return "Partial";
    case G5AuditResultUnsupported:
        return "Unsupported";
    case G5AuditResultTimeout:
        return "Timeout";
    case G5AuditResultBlocked:
        return "Blocked";
    case G5AuditResultNone:
        return "";
    }
    return "";
}

const char* g5_audit_tx_verb(G5AuditNext next) {
    switch(next) {
    case G5AuditNextStascan:
        return "STASCAN";
    case G5AuditNextCapture:
        return "CAPTURE";
    case G5AuditNextPmkid:
        return "PMKID";
    case G5AuditNextStop:
        return "";
    }
    return "";
}

G5AuditResult g5_audit_map_hs(bool quality_valid, bool have_pcap, bool have_json) {
    if(quality_valid && have_pcap && have_json) return G5AuditResultComplete;
    if(have_json) return G5AuditResultPartial;
    return G5AuditResultTimeout;
}

G5AuditResult g5_audit_map_pmkid(
    G5PmkidState state,
    const char* reason,
    bool have_22000,
    bool have_json) {
    if(state == G5PmkidValid) {
        if(have_22000 && have_json) return G5AuditResultComplete;
        if(have_json) return G5AuditResultPartial;
        return G5AuditResultTimeout;
    }
    if(state == G5PmkidPartial || state == G5PmkidDuplicate) return G5AuditResultPartial;
    if(state == G5PmkidUnsupported) {
        if(reason && strcmp(reason, "pmf-required") == 0) return G5AuditResultBlocked;
        return G5AuditResultUnsupported;
    }
    return G5AuditResultTimeout;
}
