#include "sr_test.h"

#include "sr_marauder_upload.h"

#include <string.h>

static void feed(const char* s) {
    sr_mau_up_feed(s, strlen(s));
}

static void lines(unsigned session, char* a, char* b) {
    memset(a, 0x5a, (size_t)SR_MAU_COLS + 1u);
    memset(b, 0x5a, (size_t)SR_MAU_COLS + 1u);
    sr_mau_up_lines(session, a, b);
    CHECK(strlen(a) <= (size_t)SR_MAU_COLS);
    CHECK(strlen(b) <= (size_t)SR_MAU_COLS);
    CHECK(strstr(a, "ExampleAP") == NULL);
    CHECK(strstr(b, "ExampleAP") == NULL);
}

int test_marauder_upload_run(void) {
    char a[SR_MAU_COLS + 1u];
    char b[SR_MAU_COLS + 1u];
    char c[SR_MAU_COLS + 1u];

    sr_test_failures = 0;
    sr_mau_up_reset();

    CHECK(strcmp(sr_upload_go_cmd(true), "upload -d wigle\n") == 0);
    CHECK(strcmp(sr_upload_go_cmd(false), "upload\n") == 0);
    CHECK(sr_upload_enter_cmd(true) == NULL);
    CHECK(strcmp(sr_upload_enter_cmd(false), "uploadstatus\n") == 0);
    CHECK(strstr(sr_upload_go_cmd(true), "-serial") == NULL);
    CHECK(strstr(sr_upload_go_cmd(true), "wdg") == NULL);

    CHECK(sr_mau_allow_go(0u, false) == true);
    CHECK(sr_mau_allow_go(2u, false) == true);
    CHECK(sr_mau_allow_go(1u, false) == false);
    CHECK(sr_mau_allow_go(0u, true) == false);
    CHECK(sr_mau_allow_go(2u, true) == false);

    lines(0u, a, b);
    CHECK(strcmp(a, "Ready") == 0);
    CHECK(strcmp(b, "press Upload") == 0);

    lines(1u, a, b);
    CHECK(strcmp(a, "Stop scan first") == 0);
    CHECK(strcmp(b, "then Upload") == 0);

    sr_mau_up_on_send();
    CHECK(sr_mau_up_in_flight() == true);
    lines(0u, a, b);
    CHECK(strcmp(a, "Sending") == 0);
    CHECK(strcmp(b, "keep card in") == 0);
    feed("#upload -d wigle");
    feed("Trying saved WiFi: ExampleAP");
    lines(0u, a, b);
    CHECK(strcmp(a, "Joining Wi-Fi") == 0);
    CHECK(strcmp(b, "try 1, keep card in") == 0);
    CHECK(sr_mau_up_in_flight() == true);
    feed("Could not connect to WiFi network");
    CHECK(sr_mau_up_in_flight() == true);
    feed("Trying saved WiFi: ExampleAP");
    lines(0u, a, b);
    CHECK(strcmp(a, "Joining Wi-Fi") == 0);
    CHECK(strcmp(b, "try 2, keep card in") == 0);
    CHECK(sr_mau_up_empty_list() == false);
    feed("Connected to saved WiFi: ExampleAP");
    lines(0u, a, b);
    CHECK(strcmp(a, "Wi-Fi connected") == 0);
    CHECK(strcmp(b, "keep card in") == 0);
    sr_mau_up_context(c);
    CHECK(c[0] == '\0');
    feed("Writing 1024 bytes...");
    feed("Connecting to WiFi");
    CHECK(sr_mau_up_in_flight() == true);

    feed("Upload complete");
    CHECK(sr_mau_up_in_flight() == false);
    lines(0u, a, b);
    CHECK(strcmp(a, "No logs in list") == 0);
    CHECK(strcmp(b, "nothing sent") == 0);
    CHECK(strcmp(a, "Sent to WiGLE") != 0);
    CHECK(sr_mau_up_empty_list() == true);
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Wi-Fi OK") == 0);

    /* A saved-profile read back of the SSID this app joined. Never drawn. */
    sr_mau_up_note_joined("ExampleAP");
    sr_mau_up_reset();
    sr_mau_up_context(c);
    CHECK(c[0] == '\0');
    sr_mau_up_on_send();
    feed("Trying saved WiFi: OtherAP");
    feed("Connected to saved WiFi: ExampleAPX");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Wi-Fi OK") == 0);
    sr_mau_up_on_send();
    feed("Connected to saved WiFi: ExampleAP");
    feed("Uploading wardrive_3.log...");
    feed("Upload OK");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Sent to WiGLE") == 0);
    CHECK(sr_mau_up_empty_list() == false);
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Used joined Wi-Fi") == 0);
    CHECK(strstr(c, "ExampleAP") == NULL);
    sr_mau_up_forget_joined();
    sr_mau_up_on_send();
    feed("Connected to saved WiFi: ExampleAP");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Wi-Fi OK") == 0);
    sr_mau_up_note_joined(NULL);
    sr_mau_up_reset();

    /* v1.18.0 lab tape: joinWiFi prints its dots with no newline, so the
     * saved-profile line arrives glued behind them (WiFiScan.cpp 2432, 2586). */
    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("> #upload -d wigle");
    feed("Trying saved WiFi: ExampleAP");
    feed("Connecting to WiFi...Connected to saved WiFi: ExampleAP");
    lines(0u, a, b);
    CHECK(strcmp(a, "Wi-Fi connected") == 0);
    CHECK(strcmp(b, "keep card in") == 0);
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "No logs in list") == 0);
    CHECK(strcmp(b, "nothing sent") == 0);
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Wi-Fi OK") == 0);
    sr_mau_up_note_joined("ExampleAP");
    sr_mau_up_on_send();
    feed("Connecting to WiFi.....Connected to saved WiFi: ExampleAP");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Used joined Wi-Fi") == 0);
    sr_mau_up_on_send();
    feed("Connecting to WiFiConnected to saved WiFi: ExampleAP");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Used joined Wi-Fi") == 0);
    sr_mau_up_on_send();
    feed("Connecting to WiFi...Connected to saved WiFi: ExampleAPX");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(strcmp(c, "Wi-Fi OK") == 0);
    /* Only the dot run may sit in front. Anything else is not the receipt. */
    sr_mau_up_on_send();
    feed("Connecting to WiFi..x.Connected to saved WiFi: ExampleAP");
    feed("Connecting to Wi...Connected to saved WiFi: ExampleAP");
    feed("> Connected to saved WiFi: ExampleAP");
    feed("Connecting to WiFi......");
    feed("Upload complete");
    sr_mau_up_context(c);
    CHECK(c[0] == '\0');
    sr_mau_up_forget_joined();
    sr_mau_up_reset();

    /* Check wigle.net only after a per-file WiGLE 200 receipt. */
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[UPLOAD] WiGLE upload succeeded");
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Sent to WiGLE") == 0);
    sr_mau_up_hint(c);
    CHECK(strcmp(c, "Check wigle.net") == 0);
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("Upload OK");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Sent to WiGLE") == 0);
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[UPLOAD] WiGLE already uploaded, skipping");
    feed("Upload OK");
    feed("Upload complete");
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    sr_mau_up_on_send();
    feed("Uploading wardrive_0.log...");
    feed("[UPLOAD] WiGLE upload failed");
    feed("Uploading wardrive_1.log...");
    feed("[UPLOAD] WiGLE upload succeeded");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "WiGLE rejected") == 0);
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    sr_mau_up_on_send();
    feed("Upload complete");
    sr_mau_up_hint(c);
    CHECK(c[0] == '\0');
    sr_mau_up_reset();

    /* A failed join leaves no context line. */
    sr_mau_up_on_send();
    feed("Trying saved WiFi: ExampleAP");
    feed("Could not connect to any saved WiFi network");
    sr_mau_up_context(c);
    CHECK(c[0] == '\0');
    sr_mau_up_reset();

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("There are no saved WiFi credentials");
    feed("Failed to connect to saved WiFi");
    CHECK(sr_mau_up_in_flight() == false);
    lines(0u, a, b);
    CHECK(strcmp(a, "No saved Wi-Fi") == 0);
    CHECK(strcmp(b, "Join WiFi first") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Failed to connect to saved WiFi");
    lines(2u, a, b);
    CHECK(strcmp(a, "Can't reach Wi-Fi") == 0);
    CHECK(strcmp(b, "try Join again") == 0);
    CHECK(sr_mau_up_in_flight() == false);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Could not connect to any saved WiFi network");
    feed("Failed to connect to saved WiFi");
    lines(0u, a, b);
    CHECK(strcmp(a, "Can't reach Wi-Fi") == 0);
    CHECK(strcmp(b, "try Join again") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Missing wigle credentials");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "No WiGLE key") == 0);
    CHECK(strcmp(b, "put key on card") == 0);
    CHECK(sr_mau_up_in_flight() == false);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    lines(0u, a, b);
    CHECK(strcmp(a, "Sending") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);
    feed("[UPLOAD] WiGLE upload succeeded");
    feed("Upload OK");
    CHECK(sr_mau_up_in_flight() == true);
    feed("Upload complete");
    CHECK(sr_mau_up_in_flight() == false);
    lines(0u, a, b);
    CHECK(strcmp(a, "Sent to WiGLE") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[UPLOAD] WiGLE already uploaded, skipping");
    feed("Upload OK");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Already sent") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);
    CHECK(strcmp(a, "Sent to WiGLE") != 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("Timeout reached");
    feed("[UPLOAD] WiGLE upload succeeded");
    feed("Upload OK");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Sent to WiGLE") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[UPLOAD] WiGLE upload failed");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "WiGLE rejected") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_0.log...");
    feed("WiGLE failed");
    feed("Uploading wardrive_1.log...");
    feed("[UPLOAD] WiGLE upload succeeded");
    feed("Upload OK");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "WiGLE rejected") == 0);
    CHECK(strcmp(b, "wardrive_0.log") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("Failed to connected to api.wigle.net");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Can't reach WiGLE") == 0);
    CHECK(strcmp(b, "try again later") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[WIGLE] No response bytes received");
    feed("Timeout reached");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "WiGLE timed out") == 0);
    CHECK(strcmp(b, "try again") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("File does not exist: /wardrive_2.log");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Log not on card") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("[WIGLE] Could not allocate upload buffer");
    feed("WiGLE failed");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "Board low on RAM") == 0);
    CHECK(strcmp(b, "try again") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Uploading wardrive_2.log...");
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "No confirmation") == 0);
    CHECK(strcmp(b, "wardrive_2.log") == 0);
    CHECK(strcmp(a, "Sent to WiGLE") != 0);

    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "No confirmation") == 0);

    sr_mau_up_reset();
    sr_mau_up_on_send();
    feed("Direct upload not supported");
    lines(0u, a, b);
    CHECK(strcmp(a, "Upload not in build") == 0);
    CHECK(strcmp(b, "not supported") == 0);
    CHECK(sr_mau_up_in_flight() == false);

    lines(1u, a, b);
    CHECK(strcmp(a, "Stop scan first") == 0);

    /* Boot key import, fed as the redacted copy. Not tied to a flight. */
    sr_mau_up_key_forget();
    CHECK(sr_mau_up_key_state() == SrMauKeyUnknown);
    feed("Saved WDG API Token: ***");
    CHECK(sr_mau_up_key_state() == SrMauKeyUnknown);
    feed("Saved WiGLE API Token: ***");
    CHECK(sr_mau_up_key_state() == SrMauKeyPartial);
    feed("Saved WiGLE API Name: ***");
    CHECK(sr_mau_up_key_state() == SrMauKeyImported);
    CHECK(sr_mau_up_in_flight() == false);
    sr_mau_up_reset();
    CHECK(sr_mau_up_key_state() == SrMauKeyImported);
    sr_mau_up_on_send();
    feed("Saved WiGLE API Name: ***");
    CHECK(sr_mau_up_in_flight() == true);
    feed("Upload complete");
    lines(0u, a, b);
    CHECK(strcmp(a, "No logs in list") == 0);
    sr_mau_up_key_forget();
    CHECK(sr_mau_up_key_state() == SrMauKeyUnknown);

    return sr_test_failures;
}
