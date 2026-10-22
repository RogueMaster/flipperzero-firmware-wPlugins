#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Factory Marauder v1.17.0 join setup. No furi.
 *
 * The AP table that `join -a` indexes is filled by `scanall`
 * (WIFI_SCAN_AP_STA), not by `sniffbeacon`. Strings and the fail window are
 * the v1.17.0 prints recorded in tools/host_test/fixtures/marauder_v117_join.txt.
 * A line that is not in that transcript is not a success.
 */

enum {
    SR_JOIN_AP_CAP = 16
};
enum {
    SR_JOIN_SSID_MAX = 32
};
enum {
    SR_JOIN_PASS_MIN = 8
};
enum {
    SR_JOIN_PASS_MAX = 63
};
enum {
    SR_JOIN_INDEX_MAX = 999
};
/* Bound a join -a plus non-disconnecting info completion check. */
enum {
    SR_JOIN_CHECK_TIMEOUT_MS = 30000
};
/* One dual-band sweep. channel_hop_delay defaults to 1 (WiFiScan.h:400).
 * HAS_DUAL_BAND HOP_DELAY is 250 (configs.h). dual_band_channels has 51 entries (WiFiScan.h). */
enum {
    SR_JOIN_SWEEP_MS = 51 * 250
};

enum {
    SrJoinListenOff = 0,
    SrJoinListenScan,
    SrJoinListenList,
    SrJoinListenJoin,
};

/* A completed join -a has reached WL_CONNECTED in v1.17.0. The info echo
 * is only an end-of-command barrier; it does not claim profile persistence. */
enum {
    SrJoinReceiptPending = 0,
    SrJoinReceiptFail,
    SrJoinReceiptJoined,
};

const char* sr_join_cmd_stop(void);
const char* sr_join_cmd_scan(void);
const char* sr_join_cmd_list(void);
/* `channel\n` is a state-neutral serial barrier; it does not select a saved profile. */
const char* sr_join_cmd_check(void);
/* Second screen line for a terminal failure. NULL while pending or joined. */
const char* sr_join_fail_why(unsigned receipt);

/* password is a C string. Rejects length outside 8..63 and space, '#', quotes. */
bool sr_join_password_ok(const char* password);

/* Bytes written, including the trailing newline. 0 rejects. dst is always NUL-ended when cap > 0. */
size_t sr_join_build(unsigned index, const char* password, char* dst, size_t cap);

bool sr_join_parse_ap(const char* line, size_t len, uint16_t* index, char* ssid, size_t ssid_cap);
bool sr_join_is_list_end(const char* line, size_t len);
bool sr_join_is_scan_banner(const char* line, size_t len);
bool sr_join_is_using(const char* line, size_t len);
bool sr_join_is_fail(const char* line, size_t len);

/*
 * Rewrite a `#join ... -p <secret>` echo so the secret becomes `***`.
 * line[0..*len) is the content; line must be writable through cap (NUL not required on entry).
 * On success *len shrinks or grows to the redacted length and a NUL is written when *len < cap.
 */
bool sr_join_redact_echo(char* line, size_t* len, size_t cap);

enum {
    SrJoinKeyNone = 0,
    SrJoinKeyWigleName,
    SrJoinKeyWigleToken,
    SrJoinKeyWdgToken,
};

/*
 * Boot import of /wigle_api_name.txt, /wigle_api_token.txt and /wdg_key.txt
 * prints the stored value in clear. Same contract as sr_join_redact_echo:
 * the value becomes `***` and the rest of line[0..cap) is zeroed.
 */
bool sr_join_redact_key(char* line, size_t* len, size_t cap);
/* Kind of import line, read from an already redacted copy. */
unsigned sr_join_key_kind(const char* line, size_t len);

/* GUI and apply_unknown both run under the app mutex. One listener. */
void sr_join_listen(unsigned mode);
bool sr_join_listening(void);
void sr_join_observe(const char* line, size_t len);
bool sr_join_saw_scan(void);
bool sr_join_saw_list_end(void);
bool sr_join_saw_using(void);
bool sr_join_saw_fail(void);
unsigned sr_join_receipt(void);
void sr_join_expect_index(uint16_t index);
void sr_join_expect_check(void);
size_t sr_join_ap_count(void);
bool sr_join_ap_at(size_t i, uint16_t* index, char* ssid, size_t ssid_cap);
/* Valid only after a complete, echoed list -a response. Exact index and SSID. */
bool sr_join_ap_matches(uint16_t index, const char* ssid);
/* Valid after Using SSID is observed during join. */
bool sr_join_using_matches(const char* ssid);
