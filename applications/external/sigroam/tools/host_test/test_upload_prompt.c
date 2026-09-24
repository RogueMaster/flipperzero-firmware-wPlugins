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
    ok = sr_upload_prompt(diag_seen, diag_state, up_rev, id, reason, false, out) ? 1 : 0;
    return ok;
}

int test_upload_prompt_run(void) {
    SrUploadPrompt sending;
    SrUploadPrompt uploaded;
    SrUploadPrompt not_sent;
    SrUploadPrompt p;
    const char* long_id = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
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
    CHECK(sr_upload_prompt(true, 5u, 1u, "-", NULL, false, NULL) == false);
    CHECK(fill(true, 0u, 1u, "-", "PROFILE", &p) == 1);
    CHECK(strcmp(p.line1, "Profile failed") == 0);
    CHECK(strcmp(p.line2, "no status") == 0);
    CHECK(strcmp(p.line1, "API check fail") != 0);
    CHECK(sr_upload_prompt(true, 0u, 1u, "-", "PROFILE", true, &p) == true);
    CHECK(strcmp(p.line1, "No SD") == 0);
    CHECK(strcmp(p.line2, "insert card") == 0);
    CHECK(sr_upload_prompt(true, 5u, 1u, "-", "STARTED", true, &p) == true);
    CHECK(strcmp(p.line1, "No SD") == 0);
    CHECK(strcmp(p.line2, "insert card") == 0);

    CHECK(fill(true, 5u, 2u, long_id, "HTTP_429", &p) == 1);
    CHECK(strcmp(p.line1, "WiGLE busy") == 0);
    CHECK(strcmp(p.line2, "try later") == 0);
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
    CHECK(strcmp(p.line1, "Send failed") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);
    CHECK(fill(true, 0u, 1u, "-", "NO_REPLY", &p) == 1);
    CHECK(strcmp(p.line1, "No reply") == 0);
    CHECK(strcmp(p.line2, "before upload") == 0);
    CHECK(fill(true, 5u, 1u, "20260922-04031", "NO_REPLY", &p) == 1);
    CHECK(strcmp(p.line1, "No reply") == 0);
    CHECK(strcmp(p.line2, "before upload") == 0);
    CHECK(sr_upload_prompt(true, 0u, 1u, "-", "NO_REPLY", true, &p) == true);
    CHECK(strcmp(p.line1, "No SD") == 0);
    CHECK(strcmp(p.line2, "insert card") == 0);
    CHECK(fill(true, 5u, 1u, "-", "HTTP_401", &p) == 1);
    CHECK(strcmp(p.line1, "Key rejected") == 0);
    CHECK(strcmp(p.line2, "check API") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_403", &p) == 1);
    CHECK(strcmp(p.line1, "Key rejected") == 0);
    CHECK(strcmp(p.line2, "check API") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_500", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP 500") == 0);
    CHECK(strcmp(p.line2, "not uploaded") == 0);
    CHECK(fill(true, 5u, 1u, "20260922-04031", "HTTP_404", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP 404") == 0);
    CHECK(strcmp(p.line2, "not uploaded") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_12", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP 12") == 0);
    CHECK(strcmp(p.line2, "not uploaded") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_4010", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP 4010") == 0);
    CHECK(strcmp(p.line2, "not uploaded") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP_") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HTTP_12A", &p) == 1);
    CHECK(strcmp(p.line1, "HTTP_12A") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);

    CHECK(fill(false, 0u, 1u, "-", "STARTED", &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);
    CHECK(strcmp(p.line2, "leave card in") == 0);
    CHECK(strcmp(p.line1, "Not sent") != 0);
    CHECK(fill(true, 0u, 4u, "-", "STARTED", &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);
    CHECK(strcmp(p.line2, "leave card in") == 0);
    CHECK(fill(false, 5u, 6u, "-", "STARTED", &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);
    CHECK(fill(true, 0u, 1u, "20260922-04031", "STARTED", &p) == 1);
    CHECK(strcmp(p.line1, "Sending") == 0);
    CHECK(strcmp(p.line2, "leave card in") == 0);
    CHECK(fill(true, 0u, 1u, "-", "HOME_MISS", &p) == 1);
    CHECK(strcmp(p.line1, "No home Wi-Fi") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);
    CHECK(fill(true, 5u, 1u, "20260922-04031", "PROFILE", &p) == 1);
    CHECK(strcmp(p.line1, "Profile failed") == 0);
    CHECK(strcmp(p.line2, "no status") == 0);
    CHECK(strcmp(p.line1, "API check fail") != 0);
    CHECK(fill(true, 0u, 1u, "-", "SNTP", &p) == 1);
    CHECK(strcmp(p.line1, "Clock not set") == 0);
    CHECK(fill(true, 0u, 1u, "-", "NO_PICK", &p) == 1);
    CHECK(strcmp(p.line1, "Nothing sent") == 0);
    CHECK(fill(true, 0u, 1u, "-", "EMPTY_Q", &p) == 1);
    CHECK(strcmp(p.line1, "Nothing to send") == 0);
    CHECK(strcmp(p.line2, "queue empty") == 0);
    CHECK(fill(true, 0u, 1u, "-", "SIDECAR", &p) == 1);
    CHECK(strcmp(p.line1, "Card write fail") == 0);
    CHECK(strcmp(p.line2, "may be on WiGLE") == 0);
    CHECK(fill(true, 0u, 1u, "-", "NEW_TOKEN", &p) == 1);
    CHECK(strcmp(p.line1, "NEW_TOKEN") == 0);
    CHECK(strcmp(p.line2, "press Upload") == 0);
    CHECK(fill(true, 0u, 1u, "-", "DONE", &p) == 1);
    CHECK(strcmp(p.line1, "Round finished") == 0);
    CHECK(strcmp(p.line2, "no id") == 0);
    CHECK(fill(true, 0u, 1u, "20260922-04031", "DONE", &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);
    CHECK(fill(true, 0u, 1u, "20260922-04031", "OK", &p) == 1);
    CHECK(strcmp(p.line1, "Uploaded") == 0);
    CHECK(sr_upload_prompt_is_started("STARTED") == true);
    CHECK(sr_upload_prompt_is_started("STARTEDX") == false);
    CHECK(sr_upload_prompt_is_started("START") == false);
    CHECK(sr_upload_prompt_is_started(NULL) == false);
    CHECK(sr_upload_prompt_hold(false, 0u, "STARTED") == true);
    CHECK(sr_upload_prompt_hold(true, 5u, "-") == true);
    CHECK(sr_upload_prompt_hold(true, 5u, "HTTP_429") == true);
    CHECK(sr_upload_prompt_hold(false, 5u, "-") == false);
    CHECK(sr_upload_prompt_hold(true, 0u, "-") == false);
    CHECK(sr_upload_prompt_hold(true, 0u, "HTTP_429") == false);
    CHECK(sr_upload_prompt_hold(true, 0u, "HOME_MISS") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "PROFILE") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "HOME_MISS") == false);
    CHECK(sr_upload_prompt_hold(false, 0u, "PROFILE") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "NO_REPLY") == false);
    CHECK(sr_upload_prompt_hold(true, 0u, "NO_REPLY") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "HTTP_401") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "HTTP_403") == false);
    CHECK(sr_upload_prompt_hold(true, 5u, "HTTP_500") == false);
    CHECK(sr_upload_prompt_hold(true, 0u, "HTTP_404") == false);

    CHECK(sr_upload_poll_cmd(0u, false, false, false, true) == SR_UPLOAD_POLL_STATUS);
    CHECK(sr_upload_poll_cmd(20u, true, false, false, true) == SR_UPLOAD_POLL_STATUS);
    CHECK(sr_upload_poll_cmd(100u, true, false, false, false) == SR_UPLOAD_POLL_STATUS);
    CHECK(sr_upload_poll_cmd(1u, false, false, false, true) == SR_UPLOAD_POLL_INFO);
    CHECK(sr_upload_poll_cmd(1u, false, false, true, true) == SR_UPLOAD_POLL_NONE);
    CHECK(sr_upload_poll_cmd(1u, false, true, false, true) == SR_UPLOAD_POLL_NONE);
    CHECK(sr_upload_poll_cmd(1u, true, false, false, true) == SR_UPLOAD_POLL_INFO);
    CHECK(sr_upload_poll_cmd(1u, true, false, false, false) == SR_UPLOAD_POLL_NONE);
    CHECK(sr_upload_poll_cmd(50u, true, false, false, false) == SR_UPLOAD_POLL_INFO);
    CHECK(sr_upload_poll_cmd(10u, false, true, false, true) == SR_UPLOAD_POLL_NONE);
    CHECK((unsigned)SR_UPLOAD_STATUS_PERIOD_TICKS == 20u);
    CHECK((unsigned)SR_UPLOAD_INFO_PERIOD_TICKS == 50u);

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
