#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * ★ Upload-page main sentence. Zero furi (ADR-003).
 *
 * Three English lines. Center stays Upload.
 * Priority: Diag st=5 with an id stays Uploaded.
 * Diag st=5 with no id and reason HTTP_429 is WiGLE busy / try later.
 * Diag st=5 otherwise is Sending.
 * After that, reason HTTP_429 is the same two lines.
 * Else a real id is Uploaded. Else a seen Up: whose Diag state is not 5
 * and whose id is still "-" is Not sent.
 */

enum {
    SR_UPLOAD_PROMPT_ID_COLS = 14,
    SR_UPLOAD_PROMPT_LINE = 16,
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

/* Returns false when none of the three sentences apply. out is then zeroed. */
static inline bool sr_upload_prompt_is_http_429(const char* reason) {
    static const char token[] = "HTTP_429";
    size_t i;

    if(reason == NULL) {
        return false;
    }
    for(i = 0; token[i] != '\0'; i++) {
        if(reason[i] != token[i]) {
            return false;
        }
    }
    return reason[i] == '\0';
}

static inline bool sr_upload_prompt(
    bool diag_seen,
    uint8_t diag_state,
    uint32_t up_rev,
    const char* last_trans,
    const char* reason,
    SrUploadPrompt* out) {
    if(out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if(diag_state == 5u) {
        if(sr_upload_prompt_id_present(last_trans)) {
            sr_upload_prompt_put(
                out->line1, sizeof(out->line1), "Uploaded", sizeof("Uploaded") - 1u);
            sr_upload_prompt_put(
                out->line2, sizeof(out->line2), last_trans, (size_t)SR_UPLOAD_PROMPT_ID_COLS);
            return true;
        }
        if(diag_seen && sr_upload_prompt_is_http_429(reason)) {
            sr_upload_prompt_put(
                out->line1, sizeof(out->line1), "WiGLE busy", sizeof("WiGLE busy") - 1u);
            sr_upload_prompt_put(
                out->line2, sizeof(out->line2), "try later", sizeof("try later") - 1u);
            return true;
        }
        if(diag_seen) {
            sr_upload_prompt_put(out->line1, sizeof(out->line1), "Sending", sizeof("Sending") - 1u);
            sr_upload_prompt_put(
                out->line2, sizeof(out->line2), "leave card in", sizeof("leave card in") - 1u);
            return true;
        }
    }
    if(diag_state != 5u && sr_upload_prompt_is_http_429(reason)) {
        sr_upload_prompt_put(out->line1, sizeof(out->line1), "WiGLE busy", sizeof("WiGLE busy") - 1u);
        sr_upload_prompt_put(out->line2, sizeof(out->line2), "try later", sizeof("try later") - 1u);
        return true;
    }
    if(sr_upload_prompt_id_present(last_trans)) {
        sr_upload_prompt_put(out->line1, sizeof(out->line1), "Uploaded", sizeof("Uploaded") - 1u);
        sr_upload_prompt_put(
            out->line2, sizeof(out->line2), last_trans, (size_t)SR_UPLOAD_PROMPT_ID_COLS);
        return true;
    }
    if(diag_state != 5u && up_rev != 0u) {
        sr_upload_prompt_put(out->line1, sizeof(out->line1), "Not sent", sizeof("Not sent") - 1u);
        sr_upload_prompt_put(
            out->line2, sizeof(out->line2), "press Upload", sizeof("press Upload") - 1u);
        return true;
    }
    return false;
}
