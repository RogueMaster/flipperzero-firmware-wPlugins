#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * ★ Upload-page main sentence. Zero furi (ADR-003).
 *
 * Center stays Upload. A fresh Qual sd=0 is No SD / insert card and wins
 * over every face reason. Otherwise a known face reason wins over Diag
 * st=5 and over a stale transaction id. STARTED, or Diag st=5 with no
 * settled reason, is Sending. OK / DONE with an id is Uploaded.
 * NO_REPLY is No reply / before upload. HTTP_429 is WiGLE busy.
 * HTTP_401 and HTTP_403 are Key rejected. Any other HTTP_<digits> is
 * HTTP plus that code / not uploaded. Legacy PROFILE is Profile failed /
 * no status, not an API error. Bare HTTP stays Send failed for an old
 * face that did not keep the code. Anything else already seen on Up: is
 * Not sent, or the raw token when it is not in the table.
 */

enum {
    SR_UPLOAD_POLL_NONE = 0,
    SR_UPLOAD_POLL_STATUS = 1,
    SR_UPLOAD_POLL_INFO = 2,
    /* 20 * SR_TICK_PERIOD_MS (100) = 2 s. 50 matches Dash qual refresh. */
    SR_UPLOAD_STATUS_PERIOD_TICKS = 20,
    SR_UPLOAD_INFO_PERIOD_TICKS = 50,
};

enum {
    SR_UPLOAD_PROMPT_ID_COLS = 14,
    /* 20 visible columns plus NUL. Matches the Flipper text width. */
    SR_UPLOAD_PROMPT_LINE = 21,
};

typedef struct {
    char line1[SR_UPLOAD_PROMPT_LINE];
    char line2[SR_UPLOAD_PROMPT_LINE];
} SrUploadPrompt;

_Static_assert(SR_UPLOAD_PROMPT_LINE >= SR_UPLOAD_PROMPT_ID_COLS + 1u, "id cols need a NUL");
_Static_assert(sizeof("Sending") - 1u <= 20u, "Sending exceeds 20 cols");
_Static_assert(sizeof("leave card in") - 1u <= 20u, "leave card in exceeds 20 cols");
_Static_assert(sizeof("Uploaded") - 1u <= 20u, "Uploaded exceeds 20 cols");
_Static_assert(sizeof("Not sent") - 1u <= 20u, "Not sent exceeds 20 cols");
_Static_assert(sizeof("press Upload") - 1u <= 20u, "press Upload exceeds 20 cols");
_Static_assert(sizeof("leave card in") <= SR_UPLOAD_PROMPT_LINE, "leave card in exceeds line");
_Static_assert(sizeof("press Upload") <= SR_UPLOAD_PROMPT_LINE, "press Upload exceeds line");
_Static_assert(sizeof("WiGLE busy") <= SR_UPLOAD_PROMPT_LINE, "WiGLE busy exceeds line");
_Static_assert(sizeof("try later") <= SR_UPLOAD_PROMPT_LINE, "try later exceeds line");
_Static_assert(sizeof("Not sent") <= SR_UPLOAD_PROMPT_LINE, "Not sent exceeds line");
_Static_assert(sizeof("No home Wi-Fi") <= SR_UPLOAD_PROMPT_LINE, "No home Wi-Fi exceeds line");
_Static_assert(sizeof("No reply") <= SR_UPLOAD_PROMPT_LINE, "No reply exceeds line");
_Static_assert(sizeof("before upload") <= SR_UPLOAD_PROMPT_LINE, "before upload exceeds line");
_Static_assert(sizeof("Key rejected") <= SR_UPLOAD_PROMPT_LINE, "Key rejected exceeds line");
_Static_assert(sizeof("check API") <= SR_UPLOAD_PROMPT_LINE, "check API exceeds line");
_Static_assert(sizeof("Profile failed") <= SR_UPLOAD_PROMPT_LINE, "Profile failed exceeds line");
_Static_assert(sizeof("no status") <= SR_UPLOAD_PROMPT_LINE, "no status exceeds line");
_Static_assert(sizeof("not uploaded") <= SR_UPLOAD_PROMPT_LINE, "not uploaded exceeds line");
_Static_assert(sizeof("HTTP ") - 1u + 11u <= 20u, "HTTP code exceeds 20 cols");
_Static_assert(sizeof("Nothing to send") <= SR_UPLOAD_PROMPT_LINE, "Nothing to send exceeds line");
_Static_assert(sizeof("Card write fail") <= SR_UPLOAD_PROMPT_LINE, "Card write fail exceeds line");
_Static_assert(sizeof("may be on WiGLE") <= SR_UPLOAD_PROMPT_LINE, "may be on WiGLE exceeds line");
_Static_assert(sizeof("File still open") <= SR_UPLOAD_PROMPT_LINE, "File still open exceeds line");
_Static_assert(sizeof("Round finished") <= SR_UPLOAD_PROMPT_LINE, "Round finished exceeds line");
_Static_assert(sizeof("No SD") <= SR_UPLOAD_PROMPT_LINE, "No SD exceeds line");
_Static_assert(sizeof("insert card") <= SR_UPLOAD_PROMPT_LINE, "insert card exceeds line");
_Static_assert(SR_UPLOAD_PROMPT_LINE == 21, "upload prompt line is 20 columns plus NUL");

static inline bool sr_upload_prompt_id_present(const char* id) {
    if(id == NULL || id[0] == '\0') {
        return false;
    }
    if(id[0] == '-' && id[1] == '\0') {
        return false;
    }
    return true;
}

static inline void sr_upload_prompt_put(char* dst, size_t cap, const char* src, size_t nmax) {
    size_t i;
    size_t lim;

    if(dst == NULL || cap == 0u) {
        return;
    }
    lim = cap - 1u;
    if(nmax < lim) {
        lim = nmax;
    }
    if(src == NULL) {
        dst[0] = '\0';
        return;
    }
    for(i = 0; i < lim && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static inline bool sr_upload_prompt_token_eq(const char* reason, const char* token) {
    size_t i;

    if(reason == NULL || token == NULL) {
        return false;
    }
    for(i = 0; token[i] != '\0'; i++) {
        if(reason[i] != token[i]) {
            return false;
        }
    }
    return reason[i] == '\0';
}

static inline bool sr_upload_prompt_is_http_429(const char* reason) {
    return sr_upload_prompt_token_eq(reason, "HTTP_429");
}

static inline bool sr_upload_prompt_is_started(const char* reason) {
    return sr_upload_prompt_token_eq(reason, "STARTED");
}

/* HTTP_ plus 1..11 digits and nothing else. 11 keeps the token within 16. */
static inline bool
    sr_upload_prompt_http_digits(const char* reason, const char** digits, size_t* n_out) {
    size_t i;
    size_t n;
    static const char prefix[] = "HTTP_";

    if(reason == NULL || digits == NULL || n_out == NULL) {
        return false;
    }
    for(i = 0; prefix[i] != '\0'; i++) {
        if(reason[i] != prefix[i]) {
            return false;
        }
    }
    if(reason[i] < '0' || reason[i] > '9') {
        return false;
    }
    n = 0;
    while(reason[i + n] >= '0' && reason[i + n] <= '9') {
        n++;
        if(n > 11u) {
            return false;
        }
    }
    if(n == 0u || reason[i + n] != '\0') {
        return false;
    }
    *digits = reason + i;
    *n_out = n;
    return true;
}

static inline bool sr_upload_prompt_digits_are(const char* digits, size_t n, const char* lit) {
    size_t i;

    if(digits == NULL || lit == NULL) {
        return false;
    }
    for(i = 0; lit[i] != '\0'; i++) {
        if(i >= n || digits[i] != lit[i]) {
            return false;
        }
    }
    return i == n;
}

/* Status token other than 401, 403, and 429. Those three have their own lines. */
static inline bool sr_upload_prompt_is_other_http(const char* reason) {
    const char* digits = NULL;
    size_t n = 0;

    if(!sr_upload_prompt_http_digits(reason, &digits, &n)) {
        return false;
    }
    if(sr_upload_prompt_digits_are(digits, n, "401") ||
       sr_upload_prompt_digits_are(digits, n, "403") ||
       sr_upload_prompt_digits_are(digits, n, "429")) {
        return false;
    }
    return true;
}

static inline void
    sr_upload_prompt_http_code_line(char* dst, size_t cap, const char* digits, size_t n) {
    size_t i;
    static const char head[] = "HTTP ";

    if(dst == NULL || cap == 0u) {
        return;
    }
    dst[0] = '\0';
    if(digits == NULL || n + (sizeof head) > cap) {
        return;
    }
    for(i = 0; head[i] != '\0'; i++) {
        dst[i] = head[i];
    }
    for(i = 0; i < n; i++) {
        dst[(sizeof head) - 1u + i] = digits[i];
    }
    dst[(sizeof head) - 1u + n] = '\0';
}

static inline bool
    sr_upload_prompt_fail_lines(const char* reason, const char** line1, const char** line2) {
    static const struct {
        const char* key;
        const char* line1;
        const char* line2;
    } rows[] = {
        {"HTTP_429", "WiGLE busy", "try later"},
        {"HTTP_401", "Key rejected", "check API"},
        {"HTTP_403", "Key rejected", "check API"},
        {"NO_REPLY", "No reply", "before upload"},
        {"HOME_MISS", "No home Wi-Fi", "press Upload"},
        {"SNTP", "Clock not set", "press Upload"},
        {"PROFILE", "Profile failed", "no status"},
        {"HTTP", "Send failed", "press Upload"},
        {"SIDECAR", "Card write fail", "may be on WiGLE"},
        {"NO_PICK", "Nothing sent", "press Upload"},
        {"EMPTY_Q", "Nothing to send", "queue empty"},
        {"HEADER", "Header only", "press Upload"},
        {"EMPTY", "Empty file", "press Upload"},
        {"TOO_LARGE", "File too big", "press Upload"},
        {"FAKE_GPS", "Bad GPS file", "press Upload"},
        {"ACTIVE", "File still open", "press Upload"},
        {"NULL", "Bad file", "press Upload"},
        {"ALREADY", "Already sent", "on WiGLE"},
        {"ADMIT", "File rejected", "press Upload"},
    };
    size_t i;

    if(line1 == NULL || line2 == NULL) {
        return false;
    }
    for(i = 0; i < (sizeof rows / sizeof rows[0]); i++) {
        if(!sr_upload_prompt_token_eq(reason, rows[i].key)) {
            continue;
        }
        *line1 = rows[i].line1;
        *line2 = rows[i].line2;
        return true;
    }
    return false;
}

/* Do not send another upload while this round is still open.
 * A settled failure other than HTTP_429 may be retried. HTTP_429 stays
 * held only while Diag is still st=5, matching "try later". */
static inline bool sr_upload_prompt_hold(bool diag_seen, uint8_t diag_state, const char* reason) {
    const char* line1 = NULL;
    const char* line2 = NULL;

    if(sr_upload_prompt_is_started(reason)) {
        return true;
    }
    if(!diag_seen || diag_state != 5u) {
        return false;
    }
    if(sr_upload_prompt_is_http_429(reason)) {
        return true;
    }
    if(sr_upload_prompt_fail_lines(reason, &line1, &line2)) {
        return false;
    }
    if(sr_upload_prompt_is_other_http(reason)) {
        return false;
    }
    return true;
}

/*
 * One command slot. Status keeps the 2 s cadence. Info is the other ticks:
 * empty Version gets one ident info (same idea as Dash's first info);
 * SigRoam gets one soon after enter, then every 50 ticks; generic never.
 * A tick that is also a status tick stays status.
 */
static inline int sr_upload_poll_cmd(
    uint32_t tick_n,
    bool version_known_sigroam,
    bool version_generic,
    bool ident_sent,
    bool info_armed) {
    if((tick_n % (uint32_t)SR_UPLOAD_STATUS_PERIOD_TICKS) == 0u) {
        return SR_UPLOAD_POLL_STATUS;
    }
    if(version_generic) {
        return SR_UPLOAD_POLL_NONE;
    }
    if(!version_known_sigroam) {
        if(!ident_sent) {
            return SR_UPLOAD_POLL_INFO;
        }
        return SR_UPLOAD_POLL_NONE;
    }
    if(info_armed || (tick_n % (uint32_t)SR_UPLOAD_INFO_PERIOD_TICKS) == 0u) {
        return SR_UPLOAD_POLL_INFO;
    }
    return SR_UPLOAD_POLL_NONE;
}

static inline void
    sr_upload_prompt_puts(SrUploadPrompt* out, const char* line1, const char* line2) {
    sr_upload_prompt_put(out->line1, sizeof(out->line1), line1, 20u);
    sr_upload_prompt_put(out->line2, sizeof(out->line2), line2, 20u);
}

static inline bool sr_upload_prompt_reason_plain(const char* reason) {
    if(reason == NULL || reason[0] == '\0') {
        return true;
    }
    return reason[0] == '-' && reason[1] == '\0';
}

static inline bool sr_upload_prompt_success_reason(const char* reason) {
    return sr_upload_prompt_reason_plain(reason) || sr_upload_prompt_token_eq(reason, "OK") ||
           sr_upload_prompt_token_eq(reason, "DONE");
}

static inline bool sr_upload_prompt(
    bool diag_seen,
    uint8_t diag_state,
    uint32_t up_rev,
    const char* last_trans,
    const char* reason,
    bool sd_dead,
    SrUploadPrompt* out) {
    const char* fail1 = NULL;
    const char* fail2 = NULL;

    if(out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if(sd_dead) {
        sr_upload_prompt_puts(out, "No SD", "insert card");
        return true;
    }
    if(sr_upload_prompt_fail_lines(reason, &fail1, &fail2)) {
        sr_upload_prompt_puts(out, fail1, fail2);
        return true;
    }
    if(sr_upload_prompt_is_other_http(reason)) {
        const char* digits = NULL;
        size_t n = 0;
        char line1[SR_UPLOAD_PROMPT_LINE];

        if(sr_upload_prompt_http_digits(reason, &digits, &n)) {
            sr_upload_prompt_http_code_line(line1, sizeof line1, digits, n);
            sr_upload_prompt_puts(out, line1, "not uploaded");
            return true;
        }
    }
    if(sr_upload_prompt_is_started(reason)) {
        sr_upload_prompt_puts(out, "Sending", "leave card in");
        return true;
    }
    if(sr_upload_prompt_id_present(last_trans) && sr_upload_prompt_success_reason(reason)) {
        sr_upload_prompt_put(out->line1, sizeof(out->line1), "Uploaded", sizeof("Uploaded") - 1u);
        sr_upload_prompt_put(
            out->line2, sizeof(out->line2), last_trans, (size_t)SR_UPLOAD_PROMPT_ID_COLS);
        return true;
    }
    if(diag_seen && diag_state == 5u) {
        sr_upload_prompt_puts(out, "Sending", "leave card in");
        return true;
    }
    if(sr_upload_prompt_token_eq(reason, "DONE")) {
        sr_upload_prompt_puts(out, "Round finished", "no id");
        return true;
    }
    if(sr_upload_prompt_token_eq(reason, "OK")) {
        sr_upload_prompt_puts(out, "Uploaded", "no id");
        return true;
    }
    if(!sr_upload_prompt_reason_plain(reason)) {
        sr_upload_prompt_puts(out, reason, "press Upload");
        return true;
    }
    if(diag_state != 5u && up_rev != 0u) {
        sr_upload_prompt_puts(out, "Not sent", "press Upload");
        return true;
    }
    return false;
}
