#include "sr_test.h"

#include "sr_upload_prompt.h"

#include <stdio.h>
#include <string.h>

static int fill(
    bool diag_seen,
    uint8_t diag_state,
    uint32_t up_rev,
    const char* id,
    const char* reason,
    SrUploadPrompt* out) {
    int ok;

    memset(out, 0x5a, sizeof(*out));
    ok = sr_upload_prompt(diag_seen, diag_state, up_rev, id, reason, out) ? 1 : 0;
    return ok;
}

int test_upload_prompt_run(void) {
    SrUploadPrompt sending;
    SrUploadPrompt uploaded;
    SrUploadPrompt not_sent;
    SrUploadPrompt p;
    const char* long_id =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    int sending_ok;
    int uploaded_ok;
    int not_sent_ok;

    sr_test_failures = 0;

    CHECK(strlen(long_id) == 64u);
    CHECK(SR_UPLOAD_PROMPT_ID_COLS == 14);

    sending_ok = fill(true, 5u, 4u, "-", NULL, &sending);
    CHECK(sending_ok == 1);
    CHECK(strcmp(sending.line1, "Sending") == 0);
    CHECK(strcmp(sending.line2, "leave card in") == 0);
    CHECK(strcmp(sending.line1, "Not sent") != 0);
    CHECK(strstr(sending.line1, "Not sent") == NULL);
    CHECK(strstr(sending.line2, "Not sent") == NULL);

    CHECK(fill(true, 5u, 1u, "", NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);
    CHECK(strcmp(p.line2, "leave card in") == 0);
    CHECK(fill(true, 5u, 1u, NULL, NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);

    uploaded_ok = fill(true, 5u, 2u, long_id, NULL, &uploaded);
    CHECK(uploaded_ok == 1);
    CHECK(strcmp(uploaded.line1, "Uploaded") == 0);
    CHECK(strcmp(uploaded.line2, "0123456789abcd") == 0);
    CHECK(strlen(uploaded.line2) == 14u);
    CHECK(strcmp(uploaded.line1, "Sending") != 0);

    CHECK(fill(false, 0u, 0u, "abc-XYZ.1_2", NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);
    CHECK(strcmp(p.line2, "abc-XYZ.1_2") == 0);

    CHECK(fill(true, 0u, 9u, "transid-1234567890", NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);
    CHECK(strcmp(p.line2, "transid-123456") == 0);
    CHECK(strlen(p.line2) == 14u);

    not_sent_ok = fill(true, 0u, 1u, "-", NULL, &not_sent);
    CHECK(not_sent_ok == 1);
    CHECK(strcmp(not_sent.line1, "Not sent") == 0);
    CHECK(strcmp(not_sent.line2, "press Upload") == 0);

    CHECK(fill(true, 4u, 3u, "", NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Not sent") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);
    CHECK(fill(false, 0u, 8u, NULL, NULL, &p) == 1);
    CHECK(strcmp(p.line1, "Not sent") == 0);

    CHECK(fill(true, 0u, 0u, "-", NULL, &p) == 0);
    CHECK(p.line1[0] == '\0');
    CHECK(p.line2[0] == '\0');
    CHECK(fill(true, 1u, 0u, "-", NULL, &p) == 0);
    CHECK(fill(false, 5u, 6u, "-", NULL, &p) == 0);
    CHECK(strcmp(p.line1, "Not sent") != 0);
    CHECK(strcmp(p.line1, "Sending") != 0);
    CHECK(sr_upload_prompt(true, 5u, 1u, "-", NULL, NULL) == false);

    CHECK(fill(true, 5u, 2u, long_id, "HTTP_429", &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);
    CHECK(fill(true, 5u, 1u, "-", "HTTP_429", &p) == 1);
    CHECK(strcmp(p.line1, "WiGLE busy") == 0);
    CHECK(strcmp(p.line2, "try later") == 0);
    CHECK(fill(true, 0u, 1u, "20260922-04031", "HTTP_429", &p) == 1);
    CHECK(strcmp(p.line1, "WiGLE busy") == 0);
    CHECK(strcmp(p.line2, "try later") == 0);
    CHECK(strlen("WiGLE busy") <= 15u);
    CHECK(strlen("try later") <= 15u);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_429", &p) == 1);
    CHECK(strcmp(p.line1, "WiGLE busy") == 0);
    CHECK(fill(true, 0u, 1u, "20260922-04031", "HTTP", &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);

    printf(
        "upload prompt cover: sending=%s | %s; uploaded=%s | %s; not_sent=%s | %s\n",
        sending.line1,
        sending.line2,
        uploaded.line1,
        uploaded.line2,
        not_sent.line1,
        not_sent.line2);

    return sr_test_failures;
}
